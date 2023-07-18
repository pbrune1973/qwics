/*
Qwics JDBC Client for Java

Copyright (c) 2018 Philipp Brune    Email: Philipp.Brune@hs-neu-ulm.de

This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option)
any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
for more details.

You should have received a copy of the GNU Lesser General Public License along
with this library; if not, If not, see <http://www.gnu.org/licenses/>. 

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

Neither the name of the driver nor the names of its contributors may not be
used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS  AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

package org.qwics.jdbc;

import java.lang.reflect.Field;
import java.sql.Array;
import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.ResultSet;
import java.sql.RowIdLifetime;
import java.sql.SQLException;
import java.sql.Statement;
import java.sql.Types;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.StringTokenizer;


public class QwicsDatabaseMetaData implements DatabaseMetaData {
	@Override
	public ResultSet getProcedureColumns(String catalog, String schemaPattern,
			String procedureNamePattern, String columnNamePattern)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public long getMaxLogicalLobSize() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public boolean supportsRefCursors() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	private String host;
	private int port;
	private Connection connection;
	
	public QwicsDatabaseMetaData(String host, int port, Connection con) {
		this.host = host;
		this.port = port;
		this.connection = con;
	}

	private Statement createMetaDataStatement() {
		return new QwicsStatement((QwicsConnection)connection);
	}	
	
	@Override
	public boolean allProceduresAreCallable() throws SQLException {
		return true;
	}

	@Override
	public boolean allTablesAreSelectable() throws SQLException {
		return true;
	}

	@Override
	public String getURL() throws SQLException {
		return "jdbc:qwics:"+host+":"+port;
	}

	@Override
	public String getUserName() throws SQLException {
		return null;
	}

	@Override
	public boolean isReadOnly() throws SQLException {
		return false;
	}

	@Override
	public boolean nullsAreSortedHigh() throws SQLException {
		return true;
	}

	@Override
	public boolean nullsAreSortedLow() throws SQLException {
		return false;
	}

	@Override
	public boolean nullsAreSortedAtStart() throws SQLException {
		return false;
	}

	@Override
	public boolean nullsAreSortedAtEnd() throws SQLException {
		return false;
	}

	@Override
	public String getDatabaseProductName() throws SQLException {
		return "Qwics Transaction Server";
	}

	@Override
	public String getDatabaseProductVersion() throws SQLException {
		return "0.9";
	}

	@Override
	public String getDriverName() throws SQLException {
		return "org.qwics.jdbc.QwicsDriver";
	}

	@Override
	public String getDriverVersion() throws SQLException {
		return "0.9";
	}

	@Override
	public int getDriverMajorVersion() {
		return 0;
	}

	@Override
	public int getDriverMinorVersion() {
		return 9;
	}

	@Override
	public boolean usesLocalFiles() throws SQLException {
		return false;
	}

	@Override
	public boolean usesLocalFilePerTable() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsMixedCaseIdentifiers() throws SQLException {
		return false;
	}

	@Override
	public boolean storesUpperCaseIdentifiers() throws SQLException {
		return false;
	}

	@Override
	public boolean storesLowerCaseIdentifiers() throws SQLException {
		return true;
	}

	@Override
	public boolean storesMixedCaseIdentifiers() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsMixedCaseQuotedIdentifiers() throws SQLException {
		return true;
	}

	@Override
	public boolean storesUpperCaseQuotedIdentifiers() throws SQLException {
		return false;
	}

	@Override
	public boolean storesLowerCaseQuotedIdentifiers() throws SQLException {
		return false;
	}

	@Override
	public boolean storesMixedCaseQuotedIdentifiers() throws SQLException {
		return false;
	}

	@Override
	public String getIdentifierQuoteString() throws SQLException {
		return "\"";
	}

	@Override
	public String getSQLKeywords() throws SQLException {
		return "";
	}

	@Override
	public String getNumericFunctions() throws SQLException {
		return "";
	}

	@Override
	public String getStringFunctions() throws SQLException {
		return "";
	}

	@Override
	public String getSystemFunctions() throws SQLException {
		return "";
	}

	@Override
	public String getTimeDateFunctions() throws SQLException {
		return "";
	}

	@Override
	public String getSearchStringEscape() throws SQLException {
		return "\\";
	}

	@Override
	public String getExtraNameCharacters() throws SQLException {
		return "";
	}

	@Override
	public boolean supportsAlterTableWithAddColumn() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsAlterTableWithDropColumn() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsColumnAliasing() throws SQLException {
		return true;
	}

	@Override
	public boolean nullPlusNonNullIsNull() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsConvert() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsConvert(int fromType, int toType)
			throws SQLException {
		return false;
	}

	@Override
	public boolean supportsTableCorrelationNames() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsDifferentTableCorrelationNames() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsExpressionsInOrderBy() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsOrderByUnrelated() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsGroupBy() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsGroupByUnrelated() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsGroupByBeyondSelect() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsLikeEscapeClause() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsMultipleResultSets() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsMultipleTransactions() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsNonNullableColumns() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsMinimumSQLGrammar() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsCoreSQLGrammar() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsExtendedSQLGrammar() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsANSI92EntryLevelSQL() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsANSI92IntermediateSQL() throws SQLException {
		return false;
	}

	@Override
	public boolean supportsANSI92FullSQL() throws SQLException {		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean supportsIntegrityEnhancementFacility() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsOuterJoins() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsFullOuterJoins() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsLimitedOuterJoins() throws SQLException {
		return true;
	}

	@Override
	public String getSchemaTerm() throws SQLException {
		return "schema";
	}

	@Override
	public String getProcedureTerm() throws SQLException {
		return "function";
	}

	@Override
	public String getCatalogTerm() throws SQLException {
		return "database";
	}

	@Override
	public boolean isCatalogAtStart() throws SQLException {
		return true;
	}

	@Override
	public String getCatalogSeparator() throws SQLException {
		return ".";
	}

	@Override
	public boolean supportsSchemasInDataManipulation() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsSchemasInProcedureCalls() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsSchemasInTableDefinitions() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsSchemasInIndexDefinitions() throws SQLException {
		return true;
	}

	@Override
	public boolean supportsSchemasInPrivilegeDefinitions() throws SQLException {
		return true;
	}

	public boolean supportsCatalogsInDataManipulation() throws SQLException {
	    return false;
	  }

	  public boolean supportsCatalogsInProcedureCalls() throws SQLException {
	    return false;
	  }

	  public boolean supportsCatalogsInTableDefinitions() throws SQLException {
	    return false;
	  }

	  public boolean supportsCatalogsInIndexDefinitions() throws SQLException {
	    return false;
	  }

	  public boolean supportsCatalogsInPrivilegeDefinitions() throws SQLException {
	    return false;
	  }

	  /**
	   * We support cursors for gets only it seems. I dont see a method to get a positioned delete.
	   *
	   * @return false
	   * @throws SQLException if a database access error occurs
	   */
	  public boolean supportsPositionedDelete() throws SQLException {
	    return false; // For now...
	  }

	  public boolean supportsPositionedUpdate() throws SQLException {
	    return false; // For now...
	  }

	  /**
	   * {@inheritDoc}
	   *
	   * @return true if connected to PostgreSQL 6.5+
	   */
	  public boolean supportsSelectForUpdate() throws SQLException {
	    return true;
	  }

	  public boolean supportsStoredProcedures() throws SQLException {
	    return true;
	  }

	  public boolean supportsSubqueriesInComparisons() throws SQLException {
	    return true;
	  }

	  public boolean supportsSubqueriesInExists() throws SQLException {
	    return true;
	  }

	  public boolean supportsSubqueriesInIns() throws SQLException {
	    return true;
	  }

	  public boolean supportsSubqueriesInQuantifieds() throws SQLException {
	    return true;
	  }

	  /**
	   * {@inheritDoc}
	   *
	   * @return true if connected to PostgreSQL 7.1+
	   */
	  public boolean supportsCorrelatedSubqueries() throws SQLException {
	    return true;
	  }

	  /**
	   * {@inheritDoc}
	   *
	   * @return true if connected to PostgreSQL 6.3+
	   */
	  public boolean supportsUnion() throws SQLException {
	    return true; // since 6.3
	  }

	  /**
	   * {@inheritDoc}
	   *
	   * @return true if connected to PostgreSQL 7.1+
	   */
	  public boolean supportsUnionAll() throws SQLException {
	    return true;
	  }

	  /**
	   * {@inheritDoc} In PostgreSQL, Cursors are only open within transactions.
	   */
	  public boolean supportsOpenCursorsAcrossCommit() throws SQLException {
	    return false;
	  }

	  public boolean supportsOpenCursorsAcrossRollback() throws SQLException {
	    return false;
	  }

	  /**
	   * {@inheritDoc}
	   * <p>
	   * Can statements remain open across commits? They may, but this driver cannot guarantee that. In
	   * further reflection. we are talking a Statement object here, so the answer is yes, since the
	   * Statement is only a vehicle to ExecSQL()
	   *
	   * @return true
	   */
	  public boolean supportsOpenStatementsAcrossCommit() throws SQLException {
	    return true;
	  }

	  /**
	   * {@inheritDoc}
	   * <p>
	   * Can statements remain open across rollbacks? They may, but this driver cannot guarantee that.
	   * In further contemplation, we are talking a Statement object here, so the answer is yes, since
	   * the Statement is only a vehicle to ExecSQL() in Connection
	   *
	   * @return true
	   */
	  public boolean supportsOpenStatementsAcrossRollback() throws SQLException {
	    return true;
	  }

	  public int getMaxCharLiteralLength() throws SQLException {
	    return 0; // no limit
	  }

	  public int getMaxBinaryLiteralLength() throws SQLException {
	    return 0; // no limit
	  }

	  public int getMaxColumnNameLength() throws SQLException {
	    return 20;
	  }

	  public int getMaxColumnsInGroupBy() throws SQLException {
	    return 0; // no limit
	  }

	  public int getMaxColumnsInIndex() throws SQLException {
	    return 20;
	  }

	  public int getMaxColumnsInOrderBy() throws SQLException {
	    return 0; // no limit
	  }

	  public int getMaxColumnsInSelect() throws SQLException {
	    return 0; // no limit
	  }

	  /**
	   * {@inheritDoc} What is the maximum number of columns in a table? From the CREATE TABLE reference
	   * page...
	   *
	   * <p>
	   * "The new class is created as a heap with no initial data. A class can have no more than 1600
	   * attributes (realistically, this is limited by the fact that tuple sizes must be less than 8192
	   * bytes)..."
	   *
	   * @return the max columns
	   * @throws SQLException if a database access error occurs
	   */
	  public int getMaxColumnsInTable() throws SQLException {
	    return 1600;
	  }

	  /**
	   * {@inheritDoc} How many active connection can we have at a time to this database? Well, since it
	   * depends on postmaster, which just does a listen() followed by an accept() and fork(), its
	   * basically very high. Unless the system runs out of processes, it can be 65535 (the number of
	   * aux. ports on a TCP/IP system). I will return 8192 since that is what even the largest system
	   * can realistically handle,
	   *
	   * @return the maximum number of connections
	   * @throws SQLException if a database access error occurs
	   */
	  public int getMaxConnections() throws SQLException {
	    return 8192;
	  }

	  public int getMaxCursorNameLength() throws SQLException {
	    return 20;
	  }

	  public int getMaxIndexLength() throws SQLException {
	    return 0; // no limit (larger than an int anyway)
	  }

	  public int getMaxSchemaNameLength() throws SQLException {
	    return 20;
	  }

	  public int getMaxProcedureNameLength() throws SQLException {
	    return 20;
	  }

	  public int getMaxCatalogNameLength() throws SQLException {
	    return 20;
	  }

	  public int getMaxRowSize() throws SQLException {
	    return 1073741824; // 1 GB
	  }

	  public boolean doesMaxRowSizeIncludeBlobs() throws SQLException {
	    return false;
	  }

	  public int getMaxStatementLength() throws SQLException {
	    return 0; // actually whatever fits in size_t
	  }

	  public int getMaxStatements() throws SQLException {
	    return 0;
	  }

	  public int getMaxTableNameLength() throws SQLException {
	    return 20;
	  }

	  public int getMaxTablesInSelect() throws SQLException {
	    return 0; // no limit
	  }

	  public int getMaxUserNameLength() throws SQLException {
	    return 20;
	  }

	  public int getDefaultTransactionIsolation() throws SQLException {
	    return Connection.TRANSACTION_READ_COMMITTED;
	  }

	  public boolean supportsTransactions() throws SQLException {
	    return true;
	  }

	  /**
	   * {@inheritDoc}
	   * <p>
	   * We only support TRANSACTION_SERIALIZABLE and TRANSACTION_READ_COMMITTED before 8.0; from 8.0
	   * READ_UNCOMMITTED and REPEATABLE_READ are accepted aliases for READ_COMMITTED.
	   */
	  public boolean supportsTransactionIsolationLevel(int level) throws SQLException {
	    switch (level) {
	      case Connection.TRANSACTION_READ_UNCOMMITTED:
	      case Connection.TRANSACTION_READ_COMMITTED:
	      case Connection.TRANSACTION_REPEATABLE_READ:
	      case Connection.TRANSACTION_SERIALIZABLE:
	        return true;
	      default:
	        return false;
	    }
	  }

	  public boolean supportsDataDefinitionAndDataManipulationTransactions() throws SQLException {
	    return true;
	  }

	  public boolean supportsDataManipulationTransactionsOnly() throws SQLException {
	    return false;
	  }

	  /**
	   * Does a data definition statement within a transaction force the transaction to commit? It seems
	   * to mean something like:
	   *
	   * <pre>
	   * CREATE TABLE T (A INT);
	   * INSERT INTO T (A) VALUES (2);
	   * BEGIN;
	   * UPDATE T SET A = A + 1;
	   * CREATE TABLE X (A INT);
	   * SELECT A FROM T INTO X;
	   * COMMIT;
	   * </pre>
	   *
	   * does the CREATE TABLE call cause a commit? The answer is no.
	   *
	   * @return true if so
	   * @throws SQLException if a database access error occurs
	   */
	  public boolean dataDefinitionCausesTransactionCommit() throws SQLException {
	    return false;
	  }

	  public boolean dataDefinitionIgnoredInTransactions() throws SQLException {
	    return false;
	  }

	  /**
	   * Turn the provided value into a valid string literal for direct inclusion into a query. This
	   * includes the single quotes needed around it.
	   *
	   * @param s input value
	   *
	   * @return string literal for direct inclusion into a query
	   * @throws SQLException if something wrong happens
	   */
	  protected String escapeQuotes(String s) throws SQLException {
	    StringBuilder sb = new StringBuilder();
	    sb.append("'");
	    sb.append(s);
	    sb.append("'");
	    return sb.toString();
	  }

	  public ResultSet getProcedures(String catalog, String schemaPattern, String procedureNamePattern)
	      throws SQLException {
	    String sql;
	    sql = "SELECT NULL AS PROCEDURE_CAT, n.nspname AS PROCEDURE_SCHEM, p.proname AS PROCEDURE_NAME, "
	          + "NULL, NULL, NULL, d.description AS REMARKS, "
	          + DatabaseMetaData.procedureReturnsResult + " AS PROCEDURE_TYPE, "
	          + " p.proname || '_' || p.oid AS SPECIFIC_NAME "
	          + " FROM pg_catalog.pg_namespace n, pg_catalog.pg_proc p "
	          + " LEFT JOIN pg_catalog.pg_description d ON (p.oid=d.objoid) "
	          + " LEFT JOIN pg_catalog.pg_class c ON (d.classoid=c.oid AND c.relname='pg_proc') "
	          + " LEFT JOIN pg_catalog.pg_namespace pn ON (c.relnamespace=pn.oid AND pn.nspname='pg_catalog') "
	          + " WHERE p.pronamespace=n.oid ";
	    if (schemaPattern != null && !schemaPattern.isEmpty()) {
	      sql += " AND n.nspname LIKE " + escapeQuotes(schemaPattern);
	    }
	    if (procedureNamePattern != null && !procedureNamePattern.isEmpty()) {
	      sql += " AND p.proname LIKE " + escapeQuotes(procedureNamePattern);
	    }
	    sql += " ORDER BY PROCEDURE_SCHEM, PROCEDURE_NAME, p.oid::text ";

	    return createMetaDataStatement().executeQuery(sql);
	  }

	  @Override
	  public ResultSet getTables(String catalog, String schemaPattern, String tableNamePattern,
	      String types[]) throws SQLException {
	    String select;
	    String orderby;
	    String useSchemas;
	    useSchemas = "SCHEMAS";
	    select = "SELECT NULL AS TABLE_CAT, n.nspname AS TABLE_SCHEM, c.relname AS TABLE_NAME, "
	             + " CASE n.nspname ~ '^pg_' OR n.nspname = 'information_schema' "
	             + " WHEN true THEN CASE "
	             + " WHEN n.nspname = 'pg_catalog' OR n.nspname = 'information_schema' THEN CASE c.relkind "
	             + "  WHEN 'r' THEN 'SYSTEM TABLE' "
	             + "  WHEN 'v' THEN 'SYSTEM VIEW' "
	             + "  WHEN 'i' THEN 'SYSTEM INDEX' "
	             + "  ELSE NULL "
	             + "  END "
	             + " WHEN n.nspname = 'pg_toast' THEN CASE c.relkind "
	             + "  WHEN 'r' THEN 'SYSTEM TOAST TABLE' "
	             + "  WHEN 'i' THEN 'SYSTEM TOAST INDEX' "
	             + "  ELSE NULL "
	             + "  END "
	             + " ELSE CASE c.relkind "
	             + "  WHEN 'r' THEN 'TEMPORARY TABLE' "
	             + "  WHEN 'i' THEN 'TEMPORARY INDEX' "
	             + "  WHEN 'S' THEN 'TEMPORARY SEQUENCE' "
	             + "  WHEN 'v' THEN 'TEMPORARY VIEW' "
	             + "  ELSE NULL "
	             + "  END "
	             + " END "
	             + " WHEN false THEN CASE c.relkind "
	             + " WHEN 'r' THEN 'TABLE' "
	             + " WHEN 'i' THEN 'INDEX' "
	             + " WHEN 'S' THEN 'SEQUENCE' "
	             + " WHEN 'v' THEN 'VIEW' "
	             + " WHEN 'c' THEN 'TYPE' "
	             + " WHEN 'f' THEN 'FOREIGN TABLE' "
	             + " WHEN 'm' THEN 'MATERIALIZED VIEW' "
	             + " ELSE NULL "
	             + " END "
	             + " ELSE NULL "
	             + " END "
	             + " AS TABLE_TYPE, d.description AS REMARKS "
	             + " FROM pg_catalog.pg_namespace n, pg_catalog.pg_class c "
	             + " LEFT JOIN pg_catalog.pg_description d ON (c.oid = d.objoid AND d.objsubid = 0) "
	             + " LEFT JOIN pg_catalog.pg_class dc ON (d.classoid=dc.oid AND dc.relname='pg_class') "
	             + " LEFT JOIN pg_catalog.pg_namespace dn ON (dn.oid=dc.relnamespace AND dn.nspname='pg_catalog') "
	             + " WHERE c.relnamespace = n.oid ";

	    if (schemaPattern != null && !schemaPattern.isEmpty()) {
	      select += " AND n.nspname LIKE " + escapeQuotes(schemaPattern);
	    }
	    orderby = " ORDER BY TABLE_TYPE,TABLE_SCHEM,TABLE_NAME ";

	    if (tableNamePattern != null && !tableNamePattern.isEmpty()) {
	      select += " AND c.relname LIKE " + escapeQuotes(tableNamePattern);
	    }
	    if (types != null) {
	      select += " AND (false ";
	      StringBuilder orclause = new StringBuilder();
	      for (String type : types) {
	        Map<String, String> clauses = tableTypeClauses.get(type);
	        if (clauses != null) {
	          String clause = clauses.get(useSchemas);
	          orclause.append(" OR ( ").append(clause).append(" ) ");
	        }
	      }
	      select += orclause.toString() + ") ";
	    }
	    String sql = select + orderby;

	    return createMetaDataStatement().executeQuery(sql);
	  }

	  private static final Map<String, Map<String, String>> tableTypeClauses;

	  static {
	    tableTypeClauses = new HashMap<String, Map<String, String>>();
	    Map<String, String> ht = new HashMap<String, String>();
	    tableTypeClauses.put("TABLE", ht);
	    ht.put("SCHEMAS",
	        "c.relkind = 'r' AND n.nspname !~ '^pg_' AND n.nspname <> 'information_schema'");
	    ht.put("NOSCHEMAS", "c.relkind = 'r' AND c.relname !~ '^pg_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("VIEW", ht);
	    ht.put("SCHEMAS",
	        "c.relkind = 'v' AND n.nspname <> 'pg_catalog' AND n.nspname <> 'information_schema'");
	    ht.put("NOSCHEMAS", "c.relkind = 'v' AND c.relname !~ '^pg_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("INDEX", ht);
	    ht.put("SCHEMAS",
	        "c.relkind = 'i' AND n.nspname !~ '^pg_' AND n.nspname <> 'information_schema'");
	    ht.put("NOSCHEMAS", "c.relkind = 'i' AND c.relname !~ '^pg_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("SEQUENCE", ht);
	    ht.put("SCHEMAS", "c.relkind = 'S'");
	    ht.put("NOSCHEMAS", "c.relkind = 'S'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("TYPE", ht);
	    ht.put("SCHEMAS",
	        "c.relkind = 'c' AND n.nspname !~ '^pg_' AND n.nspname <> 'information_schema'");
	    ht.put("NOSCHEMAS", "c.relkind = 'c' AND c.relname !~ '^pg_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("SYSTEM TABLE", ht);
	    ht.put("SCHEMAS",
	        "c.relkind = 'r' AND (n.nspname = 'pg_catalog' OR n.nspname = 'information_schema')");
	    ht.put("NOSCHEMAS",
	        "c.relkind = 'r' AND c.relname ~ '^pg_' AND c.relname !~ '^pg_toast_' AND c.relname !~ '^pg_temp_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("SYSTEM TOAST TABLE", ht);
	    ht.put("SCHEMAS", "c.relkind = 'r' AND n.nspname = 'pg_toast'");
	    ht.put("NOSCHEMAS", "c.relkind = 'r' AND c.relname ~ '^pg_toast_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("SYSTEM TOAST INDEX", ht);
	    ht.put("SCHEMAS", "c.relkind = 'i' AND n.nspname = 'pg_toast'");
	    ht.put("NOSCHEMAS", "c.relkind = 'i' AND c.relname ~ '^pg_toast_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("SYSTEM VIEW", ht);
	    ht.put("SCHEMAS",
	        "c.relkind = 'v' AND (n.nspname = 'pg_catalog' OR n.nspname = 'information_schema') ");
	    ht.put("NOSCHEMAS", "c.relkind = 'v' AND c.relname ~ '^pg_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("SYSTEM INDEX", ht);
	    ht.put("SCHEMAS",
	        "c.relkind = 'i' AND (n.nspname = 'pg_catalog' OR n.nspname = 'information_schema') ");
	    ht.put("NOSCHEMAS",
	        "c.relkind = 'v' AND c.relname ~ '^pg_' AND c.relname !~ '^pg_toast_' AND c.relname !~ '^pg_temp_'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("TEMPORARY TABLE", ht);
	    ht.put("SCHEMAS", "c.relkind = 'r' AND n.nspname ~ '^pg_temp_' ");
	    ht.put("NOSCHEMAS", "c.relkind = 'r' AND c.relname ~ '^pg_temp_' ");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("TEMPORARY INDEX", ht);
	    ht.put("SCHEMAS", "c.relkind = 'i' AND n.nspname ~ '^pg_temp_' ");
	    ht.put("NOSCHEMAS", "c.relkind = 'i' AND c.relname ~ '^pg_temp_' ");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("TEMPORARY VIEW", ht);
	    ht.put("SCHEMAS", "c.relkind = 'v' AND n.nspname ~ '^pg_temp_' ");
	    ht.put("NOSCHEMAS", "c.relkind = 'v' AND c.relname ~ '^pg_temp_' ");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("TEMPORARY SEQUENCE", ht);
	    ht.put("SCHEMAS", "c.relkind = 'S' AND n.nspname ~ '^pg_temp_' ");
	    ht.put("NOSCHEMAS", "c.relkind = 'S' AND c.relname ~ '^pg_temp_' ");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("FOREIGN TABLE", ht);
	    ht.put("SCHEMAS", "c.relkind = 'f'");
	    ht.put("NOSCHEMAS", "c.relkind = 'f'");
	    ht = new HashMap<String, String>();
	    tableTypeClauses.put("MATERIALIZED VIEW", ht);
	    ht.put("SCHEMAS", "c.relkind = 'm'");
	    ht.put("NOSCHEMAS", "c.relkind = 'm'");
	  }

	  @Override
	  public ResultSet getSchemas() throws SQLException {
	    return getSchemas(null, null);
	  }

	  @Override
	  public ResultSet getSchemas(String catalog, String schemaPattern) throws SQLException {
	    String sql;
	    sql = "SELECT nspname AS TABLE_SCHEM, NULL AS TABLE_CATALOG FROM pg_catalog.pg_namespace "
	          + " WHERE nspname <> 'pg_toast' AND (nspname !~ '^pg_temp_' "
	          + " OR nspname = (pg_catalog.current_schemas(true))[1]) AND (nspname !~ '^pg_toast_temp_' "
	          + " OR nspname = replace((pg_catalog.current_schemas(true))[1], 'pg_temp_', 'pg_toast_temp_')) ";
	    if (schemaPattern != null && !schemaPattern.isEmpty()) {
	      sql += " AND nspname LIKE " + escapeQuotes(schemaPattern);
	    }
	    sql += " ORDER BY TABLE_SCHEM";

	    return createMetaDataStatement().executeQuery(sql);
	  }

	  @Override
	  public ResultSet getCatalogs() throws SQLException {
		  throw new SQLException("Not implemented");
	  }

	  @Override
	  public ResultSet getTableTypes() throws SQLException {
		  throw new SQLException("Not implemented");
	  }

	  public ResultSet getColumns(String catalog, String schemaPattern, String tableNamePattern,
	                              String columnNamePattern) throws SQLException {
		  throw new SQLException("Not implemented");
	  }

	  @Override
	  public ResultSet getTablePrivileges(String catalog, String schemaPattern,
	      String tableNamePattern) throws SQLException {
		  throw new SQLException("Not implemented");
	  }

	@Override
	public <T> T unwrap(Class<T> iface) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean isWrapperFor(Class<?> iface) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public ResultSet getColumnPrivileges(String catalog, String schema,
			String table, String columnNamePattern) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getBestRowIdentifier(String catalog, String schema,
			String table, int scope, boolean nullable) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getVersionColumns(String catalog, String schema,
			String table) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getPrimaryKeys(String catalog, String schema, String table)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getImportedKeys(String catalog, String schema, String table)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getExportedKeys(String catalog, String schema, String table)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getCrossReference(String parentCatalog,
			String parentSchema, String parentTable, String foreignCatalog,
			String foreignSchema, String foreignTable) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getTypeInfo() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getIndexInfo(String catalog, String schema, String table,
			boolean unique, boolean approximate) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean supportsResultSetType(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean supportsResultSetConcurrency(int type, int concurrency)
			throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean ownUpdatesAreVisible(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean ownDeletesAreVisible(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean ownInsertsAreVisible(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean othersUpdatesAreVisible(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean othersDeletesAreVisible(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean othersInsertsAreVisible(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean updatesAreDetected(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean deletesAreDetected(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean insertsAreDetected(int type) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean supportsBatchUpdates() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public ResultSet getUDTs(String catalog, String schemaPattern,
			String typeNamePattern, int[] types) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Connection getConnection() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean supportsSavepoints() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean supportsNamedParameters() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean supportsMultipleOpenResults() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean supportsGetGeneratedKeys() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public ResultSet getSuperTypes(String catalog, String schemaPattern,
			String typeNamePattern) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getSuperTables(String catalog, String schemaPattern,
			String tableNamePattern) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getAttributes(String catalog, String schemaPattern,
			String typeNamePattern, String attributeNamePattern)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean supportsResultSetHoldability(int holdability)
			throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public int getResultSetHoldability() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getDatabaseMajorVersion() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getDatabaseMinorVersion() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getJDBCMajorVersion() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getJDBCMinorVersion() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getSQLStateType() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public boolean locatorsUpdateCopy() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean supportsStatementPooling() throws SQLException {
		return false;
	}

	@Override
	public RowIdLifetime getRowIdLifetime() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean supportsStoredFunctionsUsingCallSyntax() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean autoCommitFailureClosesAllResultSets() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public ResultSet getClientInfoProperties() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getFunctions(String catalog, String schemaPattern,
			String functionNamePattern) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getFunctionColumns(String catalog, String schemaPattern,
			String functionNamePattern, String columnNamePattern)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSet getPseudoColumns(String catalog, String schemaPattern,
			String tableNamePattern, String columnNamePattern)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean generatedKeyAlwaysReturned() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

}
