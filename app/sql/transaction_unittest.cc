// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/preprocessed/sqlite3.h"

class SQLTransactionTest : public testing::Test {
 public:
  SQLTransactionTest() {}

  void SetUp() {
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path_));
    path_ = path_.AppendASCII("SQLStatementTest.db");
    file_util::Delete(path_, false);
    ASSERT_TRUE(db_.Open(path_));

    ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  }

  void TearDown() {
    db_.Close();

    // If this fails something is going on with cleanup and later tests may
    // fail, so we want to identify problems right away.
    ASSERT_TRUE(file_util::Delete(path_, false));
  }

  sql::Connection& db() { return db_; }

  // Returns the number of rows in table "foo".
  int CountFoo() {
    sql::Statement count(db().GetUniqueStatement("SELECT count(*) FROM foo"));
    count.Step();
    return count.ColumnInt(0);
  }

 private:
  FilePath path_;
  sql::Connection db_;
};

TEST_F(SQLTransactionTest, Commit) {
  {
    sql::Transaction t(&db());
    EXPECT_FALSE(t.is_open());
    EXPECT_TRUE(t.Begin());
    EXPECT_TRUE(t.is_open());

    EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));

    t.Commit();
    EXPECT_FALSE(t.is_open());
  }

  EXPECT_EQ(1, CountFoo());
}

TEST_F(SQLTransactionTest, Rollback) {
  // Test some basic initialization, and that rollback runs when you exit the
  // scope.
  {
    sql::Transaction t(&db());
    EXPECT_FALSE(t.is_open());
    EXPECT_TRUE(t.Begin());
    EXPECT_TRUE(t.is_open());

    EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
  }

  // Nothing should have been committed since it was implicitly rolled back.
  EXPECT_EQ(0, CountFoo());

  // Test explicit rollback.
  sql::Transaction t2(&db());
  EXPECT_FALSE(t2.is_open());
  EXPECT_TRUE(t2.Begin());

  EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
  t2.Rollback();
  EXPECT_FALSE(t2.is_open());

  // Nothing should have been committed since it was explicitly rolled back.
  EXPECT_EQ(0, CountFoo());
}

// Rolling back any part of a transaction should roll back all of them.
TEST_F(SQLTransactionTest, NestedRollback) {
  EXPECT_EQ(0, db().transaction_nesting());

  // Outermost transaction.
  {
    sql::Transaction outer(&db());
    EXPECT_TRUE(outer.Begin());
    EXPECT_EQ(1, db().transaction_nesting());

    // The first inner one gets committed.
    {
      sql::Transaction inner1(&db());
      EXPECT_TRUE(inner1.Begin());
      EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      EXPECT_EQ(2, db().transaction_nesting());

      inner1.Commit();
      EXPECT_EQ(1, db().transaction_nesting());
    }

    // One row should have gotten inserted.
    EXPECT_EQ(1, CountFoo());

    // The second inner one gets rolled back.
    {
      sql::Transaction inner2(&db());
      EXPECT_TRUE(inner2.Begin());
      EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      EXPECT_EQ(2, db().transaction_nesting());

      inner2.Rollback();
      EXPECT_EQ(1, db().transaction_nesting());
    }

    // A third inner one will fail in Begin since one has already been rolled
    // back.
    EXPECT_EQ(1, db().transaction_nesting());
    {
      sql::Transaction inner3(&db());
      EXPECT_FALSE(inner3.Begin());
      EXPECT_EQ(1, db().transaction_nesting());
    }
  }
  EXPECT_EQ(0, db().transaction_nesting());
  EXPECT_EQ(0, CountFoo());
}