/* Copyright (C) 2007 - 2008 MySQL AB, 2008 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   There are special exceptions to the terms and conditions of the GPL 
   as it is applied to this software. View the full text of the 
   exception in file EXCEPTIONS-CONNECTOR-C++ in the directory of this 
   software distribution.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _MYSQL_CONNECTION_H_
#define _MYSQL_CONNECTION_H_

#include <cppconn/connection.h>
#include <list>
#include "mysql_util.h"
struct st_mysql;


namespace sql
{
namespace mysql
{


class MySQL_Savepoint : public sql::Savepoint
{
	std::string name;

public:
	MySQL_Savepoint(const std::string &savepoint);
	virtual ~MySQL_Savepoint() {}

	int getSavepointId();

	std::string &getSavepointName();

private:
	/* Prevent use of these */
	MySQL_Savepoint(const MySQL_Savepoint &);
	void operator=(MySQL_Savepoint &);
};

class MySQL_DebugLogger;
class MySQL_ConnectionData;

class CPPDBC_PUBLIC_FUNC MySQL_Connection : public sql::Connection
{
public:
	MySQL_Connection(const std::string& hostName, const std::string& userName, const std::string& password);

	MySQL_Connection(std::map<std::string, sql::ConnectPropertyVal>);

	virtual ~MySQL_Connection();

	struct ::st_mysql * getMySQLHandle();
		
	void clearWarnings();

	void close();

	void commit();

	sql::Statement *createStatement();

	bool getAutoCommit();

	std::string getCatalog();

	std::string getSchema();

	const std::string& getClientInfo(const std::string& name);

	void getClientOption(const std::string & optionName, void * optionValue);

	sql::DatabaseMetaData * getMetaData();

	enum_transaction_isolation getTransactionIsolation();

	const SQLWarning * getWarnings();

	bool isClosed();

	bool isReadOnly();

	std::string *nativeSQL(const std::string& sql);

	sql::PreparedStatement * prepareStatement(const std::string& sql);

	sql::PreparedStatement * prepareStatement(const std::string& sql, int autoGeneratedKeys);

	sql::PreparedStatement * prepareStatement(const std::string& sql, int columnIndexes[]);

	sql::PreparedStatement * prepareStatement(const std::string& sql, int resultSetType, int resultSetConcurrency);

	sql::PreparedStatement * prepareStatement(const std::string& sql, int resultSetType, int resultSetConcurrency, int resultSetHoldability);

	sql::PreparedStatement * prepareStatement(const std::string& sql, std::string columnNames[]);

	void releaseSavepoint(Savepoint * savepoint) ;

	void rollback();

	void rollback(Savepoint * savepoint);

	void setAutoCommit(bool autoCommit);

	void setCatalog(const std::string& catalog);

	void setSchema(const std::string& catalog);

	void setClientOption(const std::string & optionName, const void * optionValue);

	void setHoldability(int holdability);

	void setReadOnly(bool readOnly);

	sql::Savepoint *setSavepoint();

	sql::Savepoint *setSavepoint(const std::string& name);

	void setTransactionIsolation(enum_transaction_isolation level);


	std::string getSessionVariable(const std::string & varname);

protected:
	void checkClosed();
	void init(const std::string& hostName, const std::string& userName, const std::string& password);
	void init(std::map<std::string, sql::ConnectPropertyVal> properties);

	class MySQL_ConnectionData * intern;
private:
	/* Prevent use of these */
	MySQL_Connection(const MySQL_Connection &);
	void operator=(MySQL_Connection &);
};

}; /* namespace mysql */
}; /* namespace sql */

#endif // _MYSQL_CONNECTION_H_

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
