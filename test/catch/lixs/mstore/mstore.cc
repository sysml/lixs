#include <catch.hpp>

#include <lixs/mstore/store.hh>


TEST_CASE( "Basic CRUD operations", "[mstore]" ) {
    lixs::mstore::store store;


    SECTION( "Create entry" ) {
        bool created;
        std::string test_path = "/test/1";

        {
            REQUIRE( store.create(0, 0, test_path, created) == 0 );
            INFO( "On the first create the entry shouldn't exist so it should succeed" );
            REQUIRE( created == true );
        }

        {
            REQUIRE( store.create(0, 0, test_path, created) == 0 );
            INFO( "On the second create the entry should exist already so it should fail" );
            REQUIRE( created == false );
        }
    }

    SECTION( "Create a non-existent entry" ) {
        bool created;
        std::string read_value;
        std::string test_path = "/test/1";

        REQUIRE( store.create(0, 0, test_path, created) == 0 );
        REQUIRE( created == true );

        REQUIRE( store.read(0, 0, test_path, read_value) == 0 );

        INFO( "A new entry should be created with an empty value" );
        REQUIRE( read_value == "" );
    }

    SECTION( "Create an already existing entry" ) {
        bool created;
        std::string read_value;
        std::string test_value = "v1";
        std::string test_path = "/test/1";

        REQUIRE( store.update(0, 0, test_path, test_value) == 0 );

        REQUIRE( store.create(0, 0, test_path, created) == 0);
        REQUIRE( created == false );

        REQUIRE( store.read(0, 0, test_path, read_value) == 0);

        INFO( "Create on an existing entry shouldn't change it's value" );
        REQUIRE( read_value == test_value );
    }

    SECTION( "Read a non-existent entry" ) {
        std::string read_value;

        REQUIRE( store.read(0, 0, "/test", read_value) == ENOENT );
    }

    SECTION( "Update and read entry" ) {
        std::string read_value;
        std::string test_value = "v1";
        std::string test_path = "/test/1";

        REQUIRE( store.update(0, 0, test_path, test_value) == 0 );
        REQUIRE( store.read(0, 0, test_path, read_value) == 0 );
        REQUIRE( read_value == test_value );
    }

    SECTION( "Delete entry" ) {
        bool created;
        std::string read_value;
        std::string test_path = "/test/1";

        REQUIRE( store.create(0, 0, test_path, created) == 0 );

        REQUIRE( store.del(0, 0, test_path) == 0 );

        REQUIRE( store.read(0, 0, test_path, read_value) == ENOENT );
    }
}

static std::string permlist2str(lixs::permission_list& perms)
{
    std::stringstream ss;

    ss << "{ ";
    for (auto& p : perms) {
        ss << "{ " << p.cid << ", " << p.read << ", " << p.write << " }, ";
    }
    ss << "}";

    return ss.str();
}

TEST_CASE( "Basic permission operations" ) {
    lixs::mstore::store store;


    SECTION( "Default permissions" ) {
        bool created;
        lixs::permission_list read_perms;
        lixs::permission_list default_perms = { {0, false, false} };

        REQUIRE( store.create(0, 0, "/", created) == 0 );
        REQUIRE( created == true );

        REQUIRE( store.get_perms(0, 0, "/", read_perms) == 0 );

        INFO( "Default permissions: " << permlist2str(default_perms) );
        INFO( "   Read permissions: " << permlist2str(read_perms) );
        REQUIRE( read_perms == default_perms );
    }

    SECTION( "Set and get permissions" ) {
        bool created;
        std::string test_path = "/test";
        lixs::permission_list read_perms;
        lixs::permission_list write_perms = {{1, false, false}, {2, true, false}};

        REQUIRE( store.create(0, 0, test_path, created) == 0);

        REQUIRE( store.set_perms(0, 0, test_path, write_perms) == 0 );

        REQUIRE( store.get_perms(0, 0, test_path, read_perms) == 0 );

        /* FIXME: maybe try to get expansions to work? */
        INFO( " Set permissions: " << permlist2str(write_perms) );
        INFO( "Read permissions: " << permlist2str(read_perms) );
        REQUIRE( read_perms == write_perms );
    }
}

TEST_CASE( "Non-conflicting transactions", "[mstore][transactions]" ) {
    bool created;
    bool success;

    unsigned int tid1;
    unsigned int tid2;

    lixs::mstore::store store;


    REQUIRE( store.create(0, 0, "/test", created) == 0 );

    store.branch(tid1);

    REQUIRE( store.create(0, tid1, "/test/1", created) == 0 );
    REQUIRE( created == true );

    store.branch(tid2);

    REQUIRE( store.create(0, tid2, "/test/2", created) == 0 );
    REQUIRE( created == true );

    INFO( "Both transactions should succedd since they don't conflict" );
    REQUIRE( store.merge(tid1, success) == 0 );
    REQUIRE( success == true );

    REQUIRE( store.merge(tid2, success) == 0 );
    REQUIRE( success == true );
}

TEST_CASE( "Conflicting transactions", "[mstore][transactions]" ) {
    bool created;
    bool success;

    lixs::mstore::store store;


    SECTION( "Both transactions create the same entry" ) {
        unsigned int tid1;
        unsigned int tid2;
        std::string test_path = "/test";

        store.branch(tid1);

        REQUIRE( store.create(0, tid1, test_path, created) == 0 );
        REQUIRE( created == true );

        store.branch(tid2);

        REQUIRE( store.create(0, tid2, test_path, created) == 0 );
        REQUIRE( created == true );

        {
            REQUIRE( store.merge(tid1, success) == 0 );
            INFO( "First merge should succeed given there were no changes outside transaction" );
            REQUIRE( success == true );
        }

        {
            REQUIRE( store.merge(tid2, success) == 0 );
            INFO( "Second merge should fail given the entry was entry on previous merge" );
            REQUIRE( success == false );
        }
    }

    SECTION( "Write entry outside transaction after read in transaction" ) {
        bool success;
        unsigned int tid;
        std::string read_value;
        std::string test_value1 = "v1";
        std::string test_value2 = "v2";
        std::string test_path = "/test";

        store.branch(tid);

        REQUIRE( store.update(0, 0, test_path, test_value1) == 0 );

        /* Read value inside transaction */
        REQUIRE( store.read(0, tid, test_path, read_value) == 0);
        REQUIRE( read_value == test_value1 );

        /* Write value outside transaction */
        REQUIRE( store.update(0, 0, test_path, test_value2) == 0);

        SECTION ( "Check value changed outside of transaction" ) {
            REQUIRE( store.read(0, 0, test_path, read_value) == 0);
            REQUIRE( read_value == test_value2 );
        }

        SECTION ( "Check value didn't change inside the transaction" ) {
            REQUIRE( store.read(0, tid, test_path, read_value) == 0);
            REQUIRE( read_value == test_value1 );
        }

        /* Finally check the merge must fail */
        REQUIRE( store.merge(tid, success) == 0);
        REQUIRE( success == false );
    }
}
