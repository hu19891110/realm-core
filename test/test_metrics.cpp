/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#include "testsettings.hpp"

#if defined(TEST_METRICS)
#include "test.hpp"


// Test independence and thread-safety
// -----------------------------------
//
// All tests must be thread safe and independent of each other. This
// is required because it allows for both shuffling of the execution
// order and for parallelized testing.
//
// In particular, avoid using std::rand() since it is not guaranteed
// to be thread safe. Instead use the API offered in
// `test/util/random.hpp`.
//
// All files created in tests must use the TEST_PATH macro (or one of
// its friends) to obtain a suitable file system path. See
// `test/util/test_path.hpp`.
//
//
// Debugging and the ONLY() macro
// ------------------------------
//
// A simple way of disabling all tests except one called `Foo`, is to
// replace TEST(Foo) with ONLY(Foo) and then recompile and rerun the
// test suite. Note that you can also use filtering by setting the
// environment varible `UNITTEST_FILTER`. See `README.md` for more on
// this.
//
// Another way to debug a particular test, is to copy that test into
// `experiments/testcase.cpp` and then run `sh build.sh
// check-testcase` (or one of its friends) from the command line.

#if REALM_METRICS

#include <realm/descriptor.hpp>
#include <realm/query_expression.hpp>
#include <realm/lang_bind_helper.hpp>
#include <realm/util/encrypted_file_mapping.hpp>
#include <realm/util/to_string.hpp>
#include <realm/replication.hpp>
#include <realm/history.hpp>

#include <string>
#include <vector>

using namespace realm;
using namespace realm::metrics;
using namespace realm::test_util;
using namespace realm::util;

TEST(Metrics_HasNoReportsWhenDisabled)
{
    SHARED_GROUP_TEST_PATH(path);
    std::unique_ptr<Replication> hist(make_in_realm_history(path));
    SharedGroupOptions options(crypt_key());
    options.enable_metrics = false;
    SharedGroup sg(*hist, options);
    CHECK(!sg.get_metrics());
    Group& g = sg.begin_write();
    auto table = g.add_table("table");
    table->add_column(type_Int, "first");
    table->add_empty_row(10);
    sg.commit();
    sg.begin_read();
    table = g.get_table("table");
    CHECK(bool(table));
    Query query = table->column<int64_t>(0) == 0;
    query.count();
    sg.end_read();
    CHECK(!sg.get_metrics());
}

TEST(Metrics_HasReportsWhenEnabled)
{
    SHARED_GROUP_TEST_PATH(path);
    std::unique_ptr<Replication> hist(make_in_realm_history(path));
    SharedGroupOptions options(crypt_key());
    options.enable_metrics = true;
    SharedGroup sg(*hist, options);
    CHECK(sg.get_metrics());
    Group& g = sg.begin_write();
    auto table = g.add_table("table");
    table->add_column(type_Int, "first");
    table->add_empty_row(10);
    sg.commit();
    sg.begin_read();
    table = g.get_table("table");
    CHECK(bool(table));
    Query query = table->column<int64_t>(0) == 0;
    query.count();
    sg.end_read();
    std::shared_ptr<Metrics> metrics = sg.get_metrics();
    CHECK(metrics);
    CHECK(metrics->num_query_metrics() != 0);
    //CHECK(metrics->num_transaction_metrics() != 0);
}

TEST(Metrics_QueryTypes)
{
    SHARED_GROUP_TEST_PATH(path);
    std::unique_ptr<Replication> hist(make_in_realm_history(path));
    SharedGroupOptions options(crypt_key());
    options.enable_metrics = true;
    SharedGroup sg(*hist, options);
    CHECK(sg.get_metrics());
    Group& g = sg.begin_write();
    auto table = g.add_table("table");
    size_t int_col = table->add_column(type_Int, "col_int");
    size_t double_col = table->add_column(type_Double, "col_double");
    size_t float_col = table->add_column(type_Float, "col_float");
    size_t timestamp_col = table->add_column(type_Timestamp, "col_timestamp");
    table->add_empty_row(10);
    sg.commit();
    sg.begin_read();
    table = g.get_table("table");
    CHECK(bool(table));
    Query query = table->column<int64_t>(0) == 0;
    query.find();
    query.find_all();
    query.count();
    query.sum_int(int_col);
    query.average_int(int_col);
    query.maximum_int(int_col);
    query.minimum_int(int_col);

    query.sum_double(double_col);
    query.average_double(double_col);
    query.maximum_double(double_col);
    query.minimum_double(double_col);

    query.sum_float(float_col);
    query.average_float(float_col);
    query.maximum_float(float_col);
    query.minimum_float(float_col);

    size_t return_dummy;
    query.maximum_timestamp(timestamp_col, &return_dummy);
    query.minimum_timestamp(timestamp_col, &return_dummy);

    sg.end_read();
    std::shared_ptr<Metrics> metrics = sg.get_metrics();
    CHECK(metrics);
    CHECK_EQUAL(metrics->num_query_metrics(), 17);
    std::unique_ptr<Metrics::QueryInfoList> queries = metrics->take_queries();
    CHECK_EQUAL(metrics->num_query_metrics(), 0);
    CHECK(queries);
    CHECK_EQUAL(queries->size(), 17);
    CHECK_EQUAL(queries->at(0).get_type(), QueryInfo::type_Find);
    CHECK_EQUAL(queries->at(1).get_type(), QueryInfo::type_FindAll);
    CHECK_EQUAL(queries->at(2).get_type(), QueryInfo::type_Count);
    CHECK_EQUAL(queries->at(3).get_type(), QueryInfo::type_Sum);
    CHECK_EQUAL(queries->at(4).get_type(), QueryInfo::type_Average);
    CHECK_EQUAL(queries->at(5).get_type(), QueryInfo::type_Maximum);
    CHECK_EQUAL(queries->at(6).get_type(), QueryInfo::type_Minimum);

    CHECK_EQUAL(queries->at(7).get_type(), QueryInfo::type_Sum);
    CHECK_EQUAL(queries->at(8).get_type(), QueryInfo::type_Average);
    CHECK_EQUAL(queries->at(9).get_type(), QueryInfo::type_Maximum);
    CHECK_EQUAL(queries->at(10).get_type(), QueryInfo::type_Minimum);

    CHECK_EQUAL(queries->at(11).get_type(), QueryInfo::type_Sum);
    CHECK_EQUAL(queries->at(12).get_type(), QueryInfo::type_Average);
    CHECK_EQUAL(queries->at(13).get_type(), QueryInfo::type_Maximum);
    CHECK_EQUAL(queries->at(14).get_type(), QueryInfo::type_Minimum);

    CHECK_EQUAL(queries->at(15).get_type(), QueryInfo::type_Maximum);
    CHECK_EQUAL(queries->at(16).get_type(), QueryInfo::type_Minimum);
}

size_t find_count(std::string haystack, std::string needle)
{
    size_t find_pos = 0;
    size_t count = 0;
    while (find_pos < haystack.size()) {
        find_pos = haystack.find(needle, find_pos);
        if (find_pos == std::string::npos)
            break;
        ++find_pos;
        ++count;
    }
    return count;
}

void populate(SharedGroup& sg)
{
    Group& g = sg.begin_write();
    auto person = g.add_table("person");
    auto pet = g.add_table("pet");
    size_t age_col = person->add_column(type_Int, "age");
    size_t paid_col = person->add_column(type_Double, "paid");
    size_t weight_col = person->add_column(type_Float, "weight");
    size_t dob_col = person->add_column(type_Timestamp, "date_of_birth");
    size_t name_col = person->add_column(type_String, "name");
    size_t account_col = person->add_column(type_Bool, "account_overdue");
    size_t data_col = person->add_column(type_Binary, "data");
    size_t owes_col = person->add_column_link(type_LinkList, "owes_coffee_to", *person);

    auto create_person = [&](int age, double paid, float weight, Timestamp dob, std::string name, bool overdue,
                             std::string data, std::vector<size_t> owes_coffee_to) {
        size_t row = person->add_empty_row();
        person->set_int(age_col, row, age);
        person->set_double(paid_col, row, paid);
        person->set_float(weight_col, row, weight);
        person->set_timestamp(dob_col, row, dob);
        person->set_string(name_col, row, name);
        person->set_bool(account_col, row, overdue);
        BinaryData bd(data);
        person->set_binary(data_col, row, bd);
        for (auto ndx : owes_coffee_to)
        {
            LinkViewRef list = person->get_linklist(owes_col, row);
            list->add(ndx);
        }
    };

    create_person(27, 28.80, 170.7f, Timestamp(27, 5), "Bob", true, "e72s", {});
    create_person(28, 10.70, 165.8f, Timestamp(28, 8), "Ryan", false, "s83f", {0});
    create_person(33, 55.28, 183.3f, Timestamp(33, 3), "Cole", true, "s822k", {1, 0});
    create_person(39, 22.72, 173.8f, Timestamp(39, 2), "Nathan", true, "h282l", {1, 1, 0, 2});
    create_person(33, 29.28, 188.7f, Timestamp(33, 9), "Riley", false, "a208s", {3, 3, 2, 1});

    size_t pet_name_col = pet->add_column(type_String, "name");
    size_t pet_owner_col = pet->add_column_link(type_Link, "owner", *person);

    auto create_pet = [&](std::string name, size_t owner) {
        size_t row = pet->add_empty_row();
        pet->set_string(pet_name_col, row, name);
        pet->set_link(pet_owner_col, row, owner);
    };

    create_pet("Fido", 0);
    create_pet("Max", 1);
    create_pet("Buddy", 2);
    create_pet("Rocky", 3);
    create_pet("Toby", 3);
    create_pet("Duke", 0);

    sg.commit();
}

TEST(Metrics_QueryEqual)
{
    SHARED_GROUP_TEST_PATH(path);
    std::unique_ptr<Replication> hist(make_in_realm_history(path));
    SharedGroupOptions options(crypt_key());
    options.enable_metrics = true;
    SharedGroup sg(*hist, options);
    populate(sg);

    std::string person_table_name = "person";
    std::string pet_table_name = "pet";
    std::string query_search_term = "equal";

    Group& g = sg.begin_write();
    TableRef person = g.get_table("person");
    TableRef pet = g.get_table("pet");
    CHECK(bool(person));

    CHECK_EQUAL(person->get_column_count(), 8);
    std::vector<std::string> column_names;
    for (size_t i = 0; i < person->get_column_count(); ++i) {
        column_names.push_back(person->get_column_name(i));
    }

    Query q0 = person->column<int64_t>(0) == 0;
    Query q1 = person->column<double>(1) == 0.0;
    Query q2 = person->column<float>(2) == 0.0f;
    Query q3 = person->column<Timestamp>(3) == Timestamp(0, 0);
    StringData name("");
    Query q4 = person->column<StringData>(4) == name;
    Query q5 = person->column<bool>(5) == false;
    BinaryData bd("");
    Query q6 = person->column<BinaryData>(6) == bd;
    Query q7 = person->column<LinkList>(7) == person->get(0);
    Query q8 = pet->column<Link>(1) == person->get(0);

    q0.find_all();
    q1.find_all();
    q2.find_all();
    q3.find_all();
    q4.find_all();
    q5.find_all();
    q6.find_all();
    q7.find_all();
    q8.find_all();

    std::shared_ptr<Metrics> metrics = sg.get_metrics();
    CHECK(metrics);
    std::unique_ptr<Metrics::QueryInfoList> queries = metrics->take_queries();
    CHECK(queries);
    CHECK_EQUAL(queries->size(), 9);

    for (size_t i = 0; i < 7; ++i) {
        std::string description = queries->at(i).get_description();
        CHECK_EQUAL(find_count(description, person_table_name), 1);
        CHECK_EQUAL(find_count(description, column_names[i]), 1);
        CHECK_EQUAL(find_count(description, query_search_term), 1);
    }
    {
        std::string description = queries->at(7).get_description();
        CHECK_EQUAL(find_count(description, person_table_name), 1);
        CHECK_EQUAL(find_count(description, column_names[7]), 1);
        CHECK_EQUAL(find_count(description, "links to"), 1);
    }
    {
        std::string description = queries->at(8).get_description();
        CHECK_EQUAL(find_count(description, pet_table_name), 1);
        CHECK_EQUAL(find_count(description, "owner"), 1);
        CHECK_EQUAL(find_count(description, "links to"), 1);
    }
}

TEST(Metrics_QueryOrAndNot)
{
    SHARED_GROUP_TEST_PATH(path);
    std::unique_ptr<Replication> hist(make_in_realm_history(path));
    SharedGroupOptions options(crypt_key());
    options.enable_metrics = true;
    SharedGroup sg(*hist, options);
    populate(sg);

    std::string person_table_name = "person";
    std::string pet_table_name = "pet";
    std::string query_search_term = "equal";

    Group& g = sg.begin_write();
    TableRef person = g.get_table("person");
    TableRef pet = g.get_table("pet");
    CHECK(bool(person));

    CHECK_EQUAL(person->get_column_count(), 8);
    std::vector<std::string> column_names;
    for (size_t i = 0; i < person->get_column_count(); ++i) {
        column_names.push_back(person->get_column_name(i));
    }

    Query q0 = person->column<int64_t>(0) == 0;
    Query q1 = person->column<double>(1) == 0.0;
    Query q2 = person->column<float>(2) == 0.1f;

    Query simple_and = q0 && q1;
    Query simple_or = q0 || q1;
    Query simple_not = !q0;

    Query or_and = q2 || (simple_and);
    Query and_or = simple_and || q2;
    Query or_nested = q2 || simple_or;
    Query and_nested = q2 && simple_and;
    Query not_simple_and = !(simple_and);
    Query not_simple_or = !(simple_or);
    Query not_or_and = !(or_and);
    Query not_and_or = !(and_or);
    Query not_or_nested = !(or_nested);
    Query not_and_nested = !(and_nested);

    simple_and.find_all();
    simple_or.find_all();
    simple_not.find_all();
    or_and.find_all();
    and_or.find_all();
    or_nested.find_all();
    and_nested.find_all();
    not_simple_and.find_all();
    not_simple_or.find_all();
    not_or_and.find_all();
    not_and_or.find_all();
    not_or_nested.find_all();
    not_and_nested.find_all();

    std::shared_ptr<Metrics> metrics = sg.get_metrics();
    CHECK(metrics);
    std::unique_ptr<Metrics::QueryInfoList> queries = metrics->take_queries();
    CHECK(queries);
    CHECK_EQUAL(queries->size(), 13);

    std::string and_description = queries->at(0).get_description();
    CHECK_EQUAL(find_count(and_description, " and "), 1);
    CHECK_EQUAL(find_count(and_description, column_names[0]), 1);
    CHECK_EQUAL(find_count(and_description, column_names[1]), 1);
    CHECK_EQUAL(find_count(and_description, person_table_name), 2);
    CHECK_EQUAL(find_count(and_description, query_search_term), 2);

    std::string or_description = queries->at(1).get_description();
    CHECK_EQUAL(find_count(or_description, " or "), 1);
    CHECK_EQUAL(find_count(or_description, column_names[0]), 1);
    CHECK_EQUAL(find_count(or_description, column_names[1]), 1);
    CHECK_EQUAL(find_count(or_description, person_table_name), 2);
    CHECK_EQUAL(find_count(or_description, query_search_term), 2);

    std::string not_description = queries->at(2).get_description();
    CHECK_EQUAL(find_count(not_description, "not"), 1);
    CHECK_EQUAL(find_count(not_description, column_names[0]), 1);
    CHECK_EQUAL(find_count(not_description, person_table_name), 1);
    CHECK_EQUAL(find_count(not_description, query_search_term), 1);

    std::string or_and_description = queries->at(3).get_description();
    CHECK_EQUAL(find_count(or_and_description, and_description), 1);
    CHECK_EQUAL(find_count(or_and_description, " or "), 1);
    CHECK_EQUAL(find_count(or_and_description, column_names[2]), 1);
    CHECK_EQUAL(find_count(or_and_description, person_table_name), 3);

    std::string and_or_description = queries->at(4).get_description();
    CHECK_EQUAL(find_count(and_or_description, and_description), 1);
    CHECK_EQUAL(find_count(and_or_description, " or "), 1);
    CHECK_EQUAL(find_count(and_or_description, column_names[2]), 1);
    CHECK_EQUAL(find_count(and_or_description, person_table_name), 3);

    std::string or_nested_description = queries->at(5).get_description();
    CHECK_EQUAL(find_count(or_nested_description, or_description), 1);
    CHECK_EQUAL(find_count(or_nested_description, " or "), 2);
    CHECK_EQUAL(find_count(or_nested_description, column_names[2]), 1);
    CHECK_EQUAL(find_count(or_nested_description, person_table_name), 3);

    std::string and_nested_description = queries->at(6).get_description();
    CHECK_EQUAL(find_count(and_nested_description, and_description), 1);
    CHECK_EQUAL(find_count(and_nested_description, " and "), 2);
    CHECK_EQUAL(find_count(and_nested_description, column_names[2]), 1);
    CHECK_EQUAL(find_count(and_nested_description, person_table_name), 3);

    std::string not_simple_and_description = queries->at(7).get_description();
    CHECK_EQUAL(find_count(not_simple_and_description, and_description), 1);
    CHECK_EQUAL(find_count(not_simple_and_description, "not"), 1);

    std::string not_simple_or_description = queries->at(8).get_description();
    CHECK_EQUAL(find_count(not_simple_or_description, or_description), 1);
    CHECK_EQUAL(find_count(not_simple_or_description, "not"), 1);

    std::string not_or_and_description = queries->at(9).get_description();
    CHECK_EQUAL(find_count(not_or_and_description, or_and_description), 1);
    CHECK_EQUAL(find_count(not_or_and_description, "not"), 1);

    std::string not_and_or_description = queries->at(10).get_description();
    CHECK_EQUAL(find_count(not_and_or_description, and_or_description), 1);
    CHECK_EQUAL(find_count(not_and_or_description, "not"), 1);

    std::string not_or_nested_description = queries->at(11).get_description();
    CHECK_EQUAL(find_count(not_or_nested_description, or_nested_description), 1);
    CHECK_EQUAL(find_count(not_or_nested_description, "not"), 1);

    std::string not_and_nested_description = queries->at(12).get_description();
    CHECK_EQUAL(find_count(not_and_nested_description, and_nested_description), 1);
    CHECK_EQUAL(find_count(not_and_nested_description, "not"), 1);
}

#endif // REALM_METRICS
#endif // TEST_METRICS
