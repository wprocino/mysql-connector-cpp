/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * The MySQL Connector/C++ is licensed under the terms of the GPLv2
 * <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
 * MySQL Connectors. There are special exceptions to the terms and
 * conditions of the GPLv2 as it is applied to this software, see the
 * FLOSS License Exception
 * <http://www.mysql.com/about/legal/licensing/foss-exception.html>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <mysql_devapi.h>
#include <uuid_gen.h>

#include <time.h>
#include <sstream>
#include <forward_list>
#include <boost/format.hpp>
#include <list>

#include "impl.h"

using namespace mysqlx;
using cdk::string;
using namespace parser;


// --------------------------------------------------------------------

/*
  Table insert
  ============
*/

/*
  Internal implementation for table CRUD insert operation.

  Building on top of Op_sort<> this class implements remaining
  methods of TableInsert_impl. It stores columns and rows specified
  for the operation and presents them through cdk::Row_source and
  cdk::api::Columns interfaces.

  Overriden method Op_base::send_command() sends table insert command
  to the CDK session.
*/

class Op_table_insert
    : public Op_sort<
        internal::TableInsert_impl,
        Parser_mode::TABLE
      >
    , public cdk::Row_source
    , public cdk::api::Columns
{
  using string = cdk::string;
  using Row_list = std::forward_list < Row >;
  using Col_list = std::forward_list < string >;


  Table_ref m_table;
  Row_list  m_rows;
  Row_list::const_iterator m_cur_row = m_rows.cbegin();
  Row_list::iterator m_row_end = m_rows.before_begin();
  Col_list m_cols;
  Col_list::iterator m_col_end = m_cols.before_begin();

public:

  Op_table_insert(Table &tbl)
    : Op_sort(tbl)
    , m_table(tbl)
  {}


  void add_column(const mysqlx::string &column) override
  {
    m_col_end = m_cols.emplace_after(m_col_end, column);
  }

  Row& new_row() override
  {
    m_row_end = m_rows.emplace_after(m_row_end);
    return *m_row_end;
  }

  void add_row(const Row &row) override
  {
    m_row_end = m_rows.emplace_after(m_row_end, row);
  }

private:

  // Executable

  bool m_started;

  cdk::Reply* send_command() override
  {
    // Do nothing if no rows were specified.

    if (m_rows.empty())
      return NULL;

    // Prepare iterators to make a pass through m_rows list.
    m_started = false;
    m_row_end = m_rows.end();

    return new cdk::Reply(
      get_cdk_session().table_insert(m_table,
                                     *this,
                                     m_cols.empty() ? nullptr : this,
                                     nullptr)
                         );
  }


  // Row_source (Iterator)

  bool next() override
  {
    if (!m_started)
      m_cur_row = m_rows.cbegin();
    else
      ++m_cur_row;

    m_started = true;
    return m_cur_row != m_row_end;
  }


  // Columns

  void process(cdk::api::Columns::Processor& prc) const override
  {
    prc.list_begin();
    for (auto el : m_cols)
    {
      cdk::safe_prc(prc)->list_el()->name(el);
    }
    prc.list_end();
  }

  // Row_source (Expr_list)

  void process(cdk::Expr_list::Processor &ep) const override;

  friend mysqlx::TableInsert;
};


void TableInsert::prepare(Table &table)
{
  m_impl.reset(new Op_table_insert(table));
}


void Op_table_insert::process(cdk::Expr_list::Processor &lp) const
{
  lp.list_begin();

  for (unsigned pos = 0; pos < m_cur_row->colCount(); ++pos)
  {
    Value_expr ve((*m_cur_row)[pos], Parser_mode::TABLE);
    ve.process_if(safe_prc(lp)->list_el());
  }

  lp.list_end();
}


// --------------------------------------------------------------------

/*
  Table select
  ============
*/

/*
  Internal implementation for table CRUD select operation.

  This implementation is built from Op_select<> and Op_projection<>
  templates. It overrides Op_base::send_command() to send table select
  command to the CDK session.
*/

class Op_table_select
  : public Op_select<
      Op_projection<
        internal::TableSelect_impl,
        parser::Parser_mode::TABLE
      >,
      parser::Parser_mode::TABLE
    >
{
  //typedef cdk::string string;

  Table_ref m_table;

  cdk::Reply* send_command() override
  {
    return
        new cdk::Reply(get_cdk_session().table_select(
                          m_table,
                          get_where(),
                          get_tbl_proj(),
                          get_order_by(),
                          nullptr,
                          nullptr,
                          get_limit(),
                          get_params()
                       ));
  }


public:

  Op_table_select(Table &table)
    : Op_select(table)
    , m_table(table)
  {}

  friend mysqlx::TableSelect;
};


void TableSelect::prepare(Table &table)
{
  m_impl.reset(new Op_table_select(table));
}


// --------------------------------------------------------------------

/*
  Table update
  ============
*/

/*
  Internal implementation for table CRUD select operation.

  This implementation is built from Op_select<> and Op_projection<>
  templates and it implements the `add_set` method of TableUpdate_impl
  implemantation interface. Update requests are stored in m_set_values
  member and presented to CDK via cdk::Update_spec interface.

  It overrides Op_base::send_command() to send table update command
  to the CDK session.
*/

class Op_table_update
    : public Op_select<
        Op_projection<
          internal::TableUpdate_impl,
          parser::Parser_mode::TABLE
        >,
        parser::Parser_mode::TABLE
      >
    , public cdk::Update_spec
    , public cdk::api::Column_ref
{
  //typedef cdk::string string;
  typedef std::map<mysqlx::string, internal::ExprValue> SetValues;

  Table_ref m_table;
  std::unique_ptr<parser::Table_field_parser> m_table_field;
  SetValues m_set_values;
  SetValues::const_iterator m_set_it;

  void add_set(const mysqlx::string &field, internal::ExprValue &&val) override
  {
    m_set_values[field] = std::move(val);
  }

  cdk::Reply* send_command() override
  {
    m_set_it = m_set_values.end();

    return
        new cdk::Reply(get_cdk_session().table_update(
                        m_table,
                        get_where(),
                        *this,
                        get_order_by(),
                        get_limit(),
                        get_params()
                      ));
  }


  // cdk::Update_spec

  virtual bool next() override
  {
    if (m_set_it == m_set_values.end())
    {
      m_set_it = m_set_values.begin();
    }
    else
    {
      ++m_set_it;
    }

    bool more = m_set_it != m_set_values.end();

    if (more)
     m_table_field.reset(new parser::Table_field_parser(m_set_it->first));

    return more;
  }

  void process(cdk::Update_spec::Processor &prc) const override
  {
    prc.column(*this);

    Value_expr val_prc(m_set_it->second, parser::Parser_mode::TABLE);
    val_prc.process_if(prc.set(m_table_field->path()));
  }


  //  cdk::api::Column_ref

  const cdk::string name() const override
  {
    return m_table_field->name();
  }

  const cdk::api::Table_ref* table() const override
  {
    return m_table_field->table();
  }

public:

  Op_table_update(Table &table)
    : Op_select(table)
    , m_table(table)
  {}


  friend mysqlx::TableUpdate;
};


void TableUpdate::prepare(Table &table)
{
  m_impl.reset(new Op_table_update(table));
}


// --------------------------------------------------------------------

/*
  Table remove
  ============
*/

/*
  Internal implementation for table CRUD remove operation.

  This implementation is built using Op_select<> and Op_sort<>
  templates. It overrides Op_base::send_command() to send table remove
  command to the CDK session.
*/

class Op_table_remove
    : public Op_select<
        Op_sort<
          internal::TableRemove_impl,
          parser::Parser_mode::TABLE
        >,
        parser::Parser_mode::TABLE
      >
{
  typedef cdk::string string;

  Table_ref m_table;

  cdk::Reply* send_command() override
  {
    return
        new cdk::Reply(get_cdk_session().table_delete(
                          m_table,
                          get_where(),
                          get_order_by(),
                          get_limit(),
                          get_params()
                      ));
  }

public:

  Op_table_remove(Table &table)
    : Op_select(table)
    , m_table(table)
  {}


  friend mysqlx::TableRemove;
};


void TableRemove::prepare(Table &table)
{
  m_impl.reset(new Op_table_remove(table));
}
