/**********************************************************************
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.net
 *
 * PostGIS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * PostGIS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PostGIS.  If not, see <http://www.gnu.org/licenses/>.
 *
 **********************************************************************
 *
 * Copyright (C) 2001-2003 Refractions Research Inc.
 * Modifications Copyright (c) 2017 - Present Pivotal Software, Inc. All Rights Reserved.
 *
 **********************************************************************/


#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"

#include "../postgis_config.h"
#include "liblwgeom.h"
#include "lwgeom_transform.h"


Datum transform(PG_FUNCTION_ARGS);
Datum transform_geom(PG_FUNCTION_ARGS);
Datum postgis_proj_version(PG_FUNCTION_ARGS);



/**
 * transform( GEOMETRY, INT (output srid) )
 * tmpPts - if there is a nadgrid error (-38), we re-try the transform
 * on a copy of points.  The transformed points
 * are in an indeterminate state after the -38 error is thrown.
 */
PG_FUNCTION_INFO_V1(transform);
Datum transform(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom;
	GSERIALIZED *result=NULL;
	LWGEOM *lwgeom;
	projPJ input_pj, output_pj;
	int32 output_srid, input_srid;

	output_srid = PG_GETARG_INT32(1);
	if (output_srid == SRID_UNKNOWN)
	{
		elog(ERROR, "%d is an invalid target SRID", SRID_UNKNOWN);
		PG_RETURN_NULL();
	}

	geom = PG_GETARG_GSERIALIZED_P_COPY(0);
	input_srid = gserialized_get_srid(geom);

	if ( input_srid == SRID_UNKNOWN )
	{
		PG_FREE_IF_COPY(geom, 0);
		elog(ERROR, "Input geometry has unknown (%d) SRID", SRID_UNKNOWN);
		PG_RETURN_NULL();
	}

	/* Input SRID and output SRID are equal, noop */
	if ( input_srid == output_srid )
		PG_RETURN_POINTER(geom);

	if ( GetProjectionsUsingFCInfo(fcinfo, input_srid, output_srid, &input_pj, &output_pj) == LW_FAILURE )
	{
		PG_FREE_IF_COPY(geom, 0);
		elog(ERROR, "Failure reading projections from spatial_ref_sys.");
		PG_RETURN_NULL();
	}

	/* now we have a geometry, and input/output PJ structs. */
	lwgeom = lwgeom_from_gserialized(geom);
	lwgeom_transform(lwgeom, input_pj, output_pj);
	lwgeom->srid = output_srid;

	/* Re-compute bbox if input had one (COMPUTE_BBOX TAINTING) */
	if ( lwgeom->bbox )
	{
		lwgeom_refresh_bbox(lwgeom);
	}

	result = geometry_serialize(lwgeom);
	lwgeom_free(lwgeom);
	PG_FREE_IF_COPY(geom, 0);

	PG_RETURN_POINTER(result); /* new geometry */
}

/**
 * Transform_geom( GEOMETRY, TEXT (input proj4), TEXT (output proj4),
 *	INT (output srid)
 *
 * tmpPts - if there is a nadgrid error (-38), we re-try the transform
 * on a copy of points.  The transformed points
 * are in an indeterminate state after the -38 error is thrown.
 */
PG_FUNCTION_INFO_V1(transform_geom);
Datum transform_geom(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom;
	GSERIALIZED *result=NULL;
	LWGEOM *lwgeom;
	projPJ input_pj, output_pj;
	char *input_proj4, *output_proj4;
	text *input_proj4_text;
	text *output_proj4_text;
	int32 result_srid ;
	char *pj_errstr;

	result_srid = PG_GETARG_INT32(3);
	geom = PG_GETARG_GSERIALIZED_P_COPY(0);

	/* Set the search path if we haven't already */
	SetPROJ4LibPath();

	/* Read the arguments */
	input_proj4_text  = (PG_GETARG_TEXT_P(1));
	output_proj4_text = (PG_GETARG_TEXT_P(2));

	/* Convert from text to cstring for libproj */
	input_proj4 = text_to_cstring(input_proj4_text);
	output_proj4 = text_to_cstring(output_proj4_text);

	/* make input and output projection objects */
	input_pj = lwproj_from_string(input_proj4);
	if ( input_pj == NULL )
	{
		pj_errstr = pj_strerrno(*pj_get_errno_ref());
		if ( ! pj_errstr ) pj_errstr = "";

		/* we need this for error reporting */
		/* pfree(input_proj4); */
		pfree(output_proj4);
		pfree(geom);

		elog(ERROR,
		    "transform_geom: could not parse proj4 string '%s' %s",
		    input_proj4, pj_errstr);
		PG_RETURN_NULL();
	}
	pfree(input_proj4);

	output_pj = lwproj_from_string(output_proj4);

	if ( output_pj == NULL )
	{
		pj_errstr = pj_strerrno(*pj_get_errno_ref());
		if ( ! pj_errstr ) pj_errstr = "";

		/* we need this for error reporting */
		/* pfree(output_proj4); */
		pj_free(input_pj);
		pfree(geom);

		elog(ERROR,
			"transform_geom: couldn't parse proj4 output string: '%s': %s",
			output_proj4, pj_errstr);
		PG_RETURN_NULL();
	}
	pfree(output_proj4);

	/* now we have a geometry, and input/output PJ structs. */
	lwgeom = lwgeom_from_gserialized(geom);
	lwgeom_transform(lwgeom, input_pj, output_pj);
	lwgeom->srid = result_srid;

	/* clean up */
	pj_free(input_pj);
	pj_free(output_pj);

	/* Re-compute bbox if input had one (COMPUTE_BBOX TAINTING) */
	if ( lwgeom->bbox )
	{
		lwgeom_refresh_bbox(lwgeom);
	}

	result = geometry_serialize(lwgeom);

	lwgeom_free(lwgeom);
	PG_FREE_IF_COPY(geom, 0);

	PG_RETURN_POINTER(result); /* new geometry */
}


PG_FUNCTION_INFO_V1(postgis_proj_version);
Datum postgis_proj_version(PG_FUNCTION_ARGS)
{
	const char *ver = pj_get_release();
	text *result = cstring_to_text(ver);
	PG_RETURN_POINTER(result);
}
