<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<!-- ********************************************************************
     ********************************************************************
	 Copyright 2008, Regina Obe
	 Modifications Copyright (c) 2017 - Present Pivotal Software, Inc. All Rights Reserved.
     License: BSD
	 Purpose: This is an xsl transform that generates PostgreSQL COMMENT ON FUNCTION ddl
	 statements from postgis xml doc reference
     ******************************************************************** -->
	<xsl:output method="text" />

	<!-- We deal only with the reference chapter -->
        <xsl:template match="/">
                <xsl:apply-templates select="/book/chapter[@id='reference']" />
        </xsl:template>

        <xsl:template match="chapter">
		<xsl:variable name="ap"><xsl:text>'</xsl:text></xsl:variable>
<!-- Pull out the purpose section for each ref entry and strip whitespace and put in a variable to be tagged unto each function comment  -->
		<xsl:for-each select="sect1[not(contains(@id,'Operator') or contains(@id,'sfcgal') or contains(@id,'GUC') )]/refentry">
		  <xsl:variable name='plaincomment'>
		  	<xsl:value-of select="normalize-space(translate(translate(refnamediv/refpurpose,'&#x0d;&#x0a;', ' '), '&#09;', ' '))"/>
		  </xsl:variable>
<!-- Replace apostrophes with 2 apostrophes needed for escaping in SQL -->
		<xsl:variable name='comment'>
			<xsl:call-template name="globalReplace">
				<xsl:with-param name="outputString" select="$plaincomment"/>
				<xsl:with-param name="target" select="$ap"/>
				<xsl:with-param name="replacement" select="''"/>
			</xsl:call-template>
		</xsl:variable>
		<xsl:choose>
<!-- If this is a postgis type grab the ref entry summary and refname to make type comment -->
<xsl:when test="parent::sect1[@id='PostGIS_Types'] and not(contains(refnamediv/refname, 'raster') or contains(refnamediv/refname, 'geomval'))">
	COMMENT ON TYPE <xsl:value-of select="refnamediv/refname" /> IS 'postgis type: <xsl:value-of select='$comment' />';
</xsl:when>
		</xsl:choose>
<!-- For each function prototype generate the DDL comment statement
	If its input has the word set we no it is an aggregate
	If its input has the word winset we know it is a window function but comment signature of those are the same just have to strip off the winset
	Do not output OUT params since they define output rather than act as input and do not put a comma after argument just before an OUT parameter
	TODO:  revise case to handle any set, winset data type (right now hardcoded geometry, any element-->
		<xsl:for-each select="refsynopsisdiv/funcsynopsis/funcprototype">
COMMENT ON <xsl:choose><xsl:when test="contains(paramdef/type,' set')">AGGREGATE</xsl:when><xsl:otherwise>FUNCTION</xsl:otherwise></xsl:choose><xsl:text> </xsl:text> <xsl:value-of select="funcdef/function" />(<xsl:for-each select="paramdef"><xsl:choose><xsl:when test="count(parameter) &gt; 0">
<xsl:choose><xsl:when test="contains(parameter,'OUT')"></xsl:when><xsl:when test="contains(type,'anyelement set')">anyelement</xsl:when><xsl:when test="contains(type,'geometry set')">geometry</xsl:when><xsl:when test="contains(type,'anyelement winset')">anyelement</xsl:when><xsl:when test="contains(type,'geometry winset')">geometry</xsl:when><xsl:when test="contains(type,'geography winset')">geography</xsl:when><xsl:otherwise><xsl:value-of select="type" /></xsl:otherwise></xsl:choose><xsl:if test="position()&lt;last() and not(contains(parameter,'OUT')) and not(contains(following-sibling::paramdef[1],'OUT'))"><xsl:text>, </xsl:text></xsl:if></xsl:when>
</xsl:choose></xsl:for-each>) IS '<xsl:call-template name="listparams"><xsl:with-param name="func" select="." /></xsl:call-template> <xsl:value-of select='$comment' />';
			</xsl:for-each>
		</xsl:for-each>
	</xsl:template>

<!--General replace macro hack to make up for the fact xsl 1.0 does not have a built in one.
	Not needed for xsl 2.0 lifted from http://www.xml.com/pub/a/2002/06/05/transforming.html -->
	<xsl:template name="globalReplace">
	  <xsl:param name="outputString"/>
	  <xsl:param name="target"/>
	  <xsl:param name="replacement"/>
	  <xsl:choose>
		<xsl:when test="contains($outputString,$target)">
		  <xsl:value-of select=
			"concat(substring-before($outputString,$target),
				   $replacement)"/>
		  <xsl:call-template name="globalReplace">
			<xsl:with-param name="outputString"
				 select="substring-after($outputString,$target)"/>
			<xsl:with-param name="target" select="$target"/>
			<xsl:with-param name="replacement"
				 select="$replacement"/>
		  </xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
		  <xsl:value-of select="$outputString"/>
		</xsl:otherwise>
	  </xsl:choose>
	</xsl:template>

	<!--macro to pull out function parameter names so we can provide a pretty arg list prefix for each function.  -->
	<xsl:template name="listparams">
		<xsl:param name="func" />
		<xsl:for-each select="$func">
			<xsl:if test="count(paramdef/parameter) &gt; 0">args: </xsl:if>
			<xsl:for-each select="paramdef">
				<xsl:choose>
					<xsl:when test="count(parameter) &gt; 0">
						<xsl:value-of select="parameter" />
					</xsl:when>
				</xsl:choose>
				<xsl:if test="position()&lt;last()"><xsl:text>, </xsl:text></xsl:if>
			</xsl:for-each>
			<xsl:if test="count(paramdef/parameter) &gt; 0"> - </xsl:if>
		</xsl:for-each>
	</xsl:template>

</xsl:stylesheet>
