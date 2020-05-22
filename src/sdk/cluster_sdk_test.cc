/*
 * cluster_sdk_test.cc
 * Copyright (C) 4paradigm.com 2020 wangtaize <wangtaize@4paradigm.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sched.h>
#include <unistd.h>
#include "sdk/cluster_sdk.h"
#include "gtest/gtest.h"
#include "brpc/server.h"
#include "gflags/gflags.h"
#include "base/file_util.h"
#include "client/ns_client.h"
#include "gtest/gtest.h"
#include "base/glog_wapper.h" // NOLINT
#include "nameserver/name_server_impl.h"
#include "proto/name_server.pb.h"
#include "proto/tablet.pb.h"
#include "proto/type.pb.h"
#include "rpc/rpc_client.h"
#include "tablet/tablet_impl.h"
#include "timer.h" // NOLINT
#include "sdk/mini_cluster.h"


namespace rtidb {
namespace sdk {

typedef ::google::protobuf::RepeatedPtrField<::rtidb::common::ColumnDesc>
    RtiDBSchema;
typedef ::google::protobuf::RepeatedPtrField<::rtidb::common::ColumnKey>
    RtiDBIndex;
inline std::string GenRand() { return std::to_string(rand() % 10000000 + 1); } // NOLINT

class MockClosure : public ::google::protobuf::Closure {
 public:
    MockClosure() {}
    ~MockClosure() {}
    void Run() {}
};

class ClusterSDKTest : public ::testing::Test {
 public:
    ClusterSDKTest():mc_(6181) {}
    ~ClusterSDKTest() {}
    void SetUp() {
        bool ok = mc_.SetUp();
        ASSERT_TRUE(ok);
    }
    void TearDown() {
        mc_.Close();
    }
 public:
    MiniCluster mc_;
};

TEST_F(ClusterSDKTest, smoketest) {
    ::rtidb::nameserver::TableInfo table_info;
    table_info.set_format_version(1);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    auto ns_client = mc_.GetNsClient();
    std::string error;
    bool ok = ns_client->CreateDatabase(db, error);
    ASSERT_TRUE(ok);
    table_info.set_name(name);
    table_info.set_db(db);
    RtiDBSchema* schema = table_info.mutable_column_desc_v1();
    auto col1 = schema->Add();
    col1->set_name("col1");
    col1->set_data_type(::rtidb::type::kVarchar);
    col1->set_type("string");
    auto col2 = schema->Add();
    col2->set_name("col2");
    col2->set_data_type(::rtidb::type::kBigInt);
    col2->set_type("int64");
    col2->set_is_ts_col(true);
    RtiDBIndex* index = table_info.mutable_column_key();
    auto key1 = index->Add();
    key1->set_index_name("index0");
    key1->add_col_name("col1");
    key1->add_ts_name("col2");
    ok = ns_client->CreateTable(table_info, error);
    ASSERT_TRUE(ok);
    ClusterOptions option;
    option.zk_cluster = mc_.GetZkCluster();
    option.zk_path =mc_.GetZkPath();
    ClusterSDK sdk(option);
    ASSERT_TRUE(sdk.Init());
    std::vector<std::shared_ptr<::rtidb::client::TabletClient>> tablet;
    ok = sdk.GetTabletByTable(db, name, &tablet);
    ASSERT_TRUE(ok);
    ASSERT_EQ(1, tablet.size());
    uint32_t tid = sdk.GetTableId(db, name);
    ASSERT_TRUE(tid != 0);
    auto table_ptr = sdk.GetTableInfo(db, name);
    ASSERT_EQ(table_ptr->db(), db);
    ASSERT_EQ(table_ptr->name(), name);
}

}  // sdk
}  // rtidb

int main(int argc, char** argv) {
    FLAGS_zk_session_timeout = 100000;
    ::testing::InitGoogleTest(&argc, argv);
    srand(time(NULL));
    ::rtidb::base::SetLogLevel(INFO);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    return RUN_ALL_TESTS();
}


