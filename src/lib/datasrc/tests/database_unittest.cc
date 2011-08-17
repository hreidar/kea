// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <gtest/gtest.h>

#include <dns/name.h>
#include <dns/rrttl.h>
#include <dns/rrset.h>
#include <exceptions/exceptions.h>

#include <datasrc/database.h>
#include <datasrc/zone.h>
#include <datasrc/data_source.h>
#include <datasrc/iterator.h>

#include <testutils/dnsmessage_test.h>

#include <map>

using namespace isc::datasrc;
using namespace std;
using namespace boost;
using namespace isc::dns;

namespace {

/*
 * An accessor with minimum implementation, keeping the original
 * "NotImplemented" methods.
 */
class NopAccessor : public DatabaseAccessor {
public:
    NopAccessor() : database_name_("mock_database")
    { }

    virtual std::pair<bool, int> getZone(const Name& name) const {
        if (name == Name("example.org")) {
            return (std::pair<bool, int>(true, 42));
        } else if (name == Name("null.example.org")) {
            return (std::pair<bool, int>(true, 13));
        } else if (name == Name("empty.example.org")) {
            return (std::pair<bool, int>(true, 0));
        } else if (name == Name("bad.example.org")) {
            return (std::pair<bool, int>(true, -1));
        } else {
            return (std::pair<bool, int>(false, 0));
        }
    }

    virtual const std::string& getDBName() const {
        return (database_name_);
    }

    // These are just to compile, they won't be called
    virtual void searchForRecords(int, const std::string&) { }
    virtual bool getNextRecord(string*, size_t) {
        return (false);
    }
    virtual void resetSearch() { }
private:
    const std::string database_name_;

};

/*
 * A virtual database connection that pretends it contains single zone --
 * example.org.
 *
 * It has the same getZone method as NopConnection, but it provides
 * implementation of the optional functionality.
 */
class MockAccessor : public NopAccessor {
public:
    MockAccessor() : search_running_(false)
    {
        fillData();
    }
private:
    class MockIteratorContext : public IteratorContext {
    private:
        int step;
    public:
        MockIteratorContext() :
            step(0)
        { }
        virtual bool getNext(string data[], size_t size) {
            if (size != DatabaseAccessor::COLUMN_COUNT) {
                isc_throw(DataSourceError, "Wrong column count in getNextRecord");
            }
            switch (step ++) {
                case 0:
                    data[DatabaseAccessor::NAME_COLUMN] = "example.org";
                    data[DatabaseAccessor::TYPE_COLUMN] = "SOA";
                    data[DatabaseAccessor::TTL_COLUMN] = "300";
                    data[DatabaseAccessor::RDATA_COLUMN] = "ns1.example.org. admin.example.org. "
                        "1234 3600 1800 2419200 7200";
                    return (true);
                case 1:
                    data[DatabaseAccessor::NAME_COLUMN] = "x.example.org";
                    data[DatabaseAccessor::TYPE_COLUMN] = "A";
                    data[DatabaseAccessor::TTL_COLUMN] = "300";
                    data[DatabaseAccessor::RDATA_COLUMN] = "192.0.2.1";
                    return (true);
                case 2:
                    data[DatabaseAccessor::NAME_COLUMN] = "x.example.org";
                    data[DatabaseAccessor::TYPE_COLUMN] = "A";
                    data[DatabaseAccessor::TTL_COLUMN] = "300";
                    data[DatabaseAccessor::RDATA_COLUMN] = "192.0.2.2";
                    return (true);
                case 3:
                    data[DatabaseAccessor::NAME_COLUMN] = "x.example.org";
                    data[DatabaseAccessor::TYPE_COLUMN] = "AAAA";
                    data[DatabaseAccessor::TTL_COLUMN] = "300";
                    data[DatabaseAccessor::RDATA_COLUMN] = "2001:db8::1";
                    return (true);
                case 4:
                    data[DatabaseAccessor::NAME_COLUMN] = "x.example.org";
                    data[DatabaseAccessor::TYPE_COLUMN] = "AAAA";
                    data[DatabaseAccessor::TTL_COLUMN] = "300";
                    data[DatabaseAccessor::RDATA_COLUMN] = "2001:db8::2";
                    return (true);
                default:
                    ADD_FAILURE() <<
                        "Request past the end of iterator context";
                case 5:
                    return (false);
            }
        }
    };
    class EmptyIteratorContext : public IteratorContext {
    public:
        virtual bool getNext(string[], size_t size) {
            if (size != DatabaseAccessor::COLUMN_COUNT) {
                isc_throw(DataSourceError, "Wrong column count in getNextRecord");
            }
            return (false);
        }
    };
    class BadIteratorContext : public IteratorContext {
    private:
        int step;
    public:
        BadIteratorContext() :
            step(0)
        { }
        virtual bool getNext(string data[], size_t size) {
            if (size != DatabaseAccessor::COLUMN_COUNT) {
                isc_throw(DataSourceError, "Wrong column count in getNextRecord");
            }
            switch (step ++) {
                case 0:
                    data[DatabaseAccessor::NAME_COLUMN] = "x.example.org";
                    data[DatabaseAccessor::TYPE_COLUMN] = "A";
                    data[DatabaseAccessor::TTL_COLUMN] = "300";
                    data[DatabaseAccessor::RDATA_COLUMN] = "192.0.2.1";
                    return (true);
                case 1:
                    data[DatabaseAccessor::NAME_COLUMN] = "x.example.org";
                    data[DatabaseAccessor::TYPE_COLUMN] = "A";
                    data[DatabaseAccessor::TTL_COLUMN] = "301";
                    data[DatabaseAccessor::RDATA_COLUMN] = "192.0.2.2";
                    return (true);
                default:
                    ADD_FAILURE() <<
                        "Request past the end of iterator context";
                case 2:
                    return (false);
            }
        }
    };
public:
    virtual IteratorContextPtr getAllRecords(const Name&, int id) const {
        if (id == 42) {
            return (IteratorContextPtr(new MockIteratorContext()));
        } else if (id == 13) {
            return (IteratorContextPtr());
        } else if (id == 0) {
            return (IteratorContextPtr(new EmptyIteratorContext()));
        } else if (id == -1) {
            return (IteratorContextPtr(new BadIteratorContext()));
        } else {
            isc_throw(isc::Unexpected, "Unknown zone ID");
        }
    }

    virtual void searchForRecords(int zone_id, const std::string& name) {
        search_running_ = true;

        // 'hardcoded' name to trigger exceptions (for testing
        // the error handling of find() (the other on is below in
        // if the name is "exceptiononsearch" it'll raise an exception here
        if (name == "dsexception.in.search.") {
            isc_throw(DataSourceError, "datasource exception on search");
        } else if (name == "iscexception.in.search.") {
            isc_throw(isc::Exception, "isc exception on search");
        } else if (name == "basicexception.in.search.") {
            throw std::exception();
        }
        searched_name_ = name;

        // we're not aiming for efficiency in this test, simply
        // copy the relevant vector from records
        cur_record = 0;
        if (zone_id == 42) {
            if (records.count(name) > 0) {
                cur_name = records.find(name)->second;
            } else {
                cur_name.clear();
            }
        } else {
            cur_name.clear();
        }
    };

    virtual bool getNextRecord(std::string columns[], size_t column_count) {
        if (searched_name_ == "dsexception.in.getnext.") {
            isc_throw(DataSourceError, "datasource exception on getnextrecord");
        } else if (searched_name_ == "iscexception.in.getnext.") {
            isc_throw(isc::Exception, "isc exception on getnextrecord");
        } else if (searched_name_ == "basicexception.in.getnext.") {
            throw std::exception();
        }

        if (column_count != DatabaseAccessor::COLUMN_COUNT) {
            isc_throw(DataSourceError, "Wrong column count in getNextRecord");
        }
        if (cur_record < cur_name.size()) {
            for (size_t i = 0; i < column_count; ++i) {
                columns[i] = cur_name[cur_record][i];
            }
            cur_record++;
            return (true);
        } else {
            resetSearch();
            return (false);
        }
    };

    virtual void resetSearch() {
        search_running_ = false;
    };

    bool searchRunning() const {
        return (search_running_);
    }
private:
    std::map<std::string, std::vector< std::vector<std::string> > > records;
    // used as internal index for getNextRecord()
    size_t cur_record;
    // used as temporary storage after searchForRecord() and during
    // getNextRecord() calls, as well as during the building of the
    // fake data
    std::vector< std::vector<std::string> > cur_name;

    // This boolean is used to make sure find() calls resetSearch
    // when it encounters an error
    bool search_running_;

    // We store the name passed to searchForRecords, so we can
    // hardcode some exceptions into getNextRecord
    std::string searched_name_;

    // Adds one record to the current name in the database
    // The actual data will not be added to 'records' until
    // addCurName() is called
    void addRecord(const std::string& name,
                   const std::string& type,
                   const std::string& sigtype,
                   const std::string& rdata) {
        std::vector<std::string> columns;
        columns.push_back(name);
        columns.push_back(type);
        columns.push_back(sigtype);
        columns.push_back(rdata);
        cur_name.push_back(columns);
    }

    // Adds all records we just built with calls to addRecords
    // to the actual fake database. This will clear cur_name,
    // so we can immediately start adding new records.
    void addCurName(const std::string& name) {
        ASSERT_EQ(0, records.count(name));
        // Append the name to all of them
        for (std::vector<std::vector<std::string> >::iterator
             i(cur_name.begin()); i != cur_name.end(); ++ i) {
            i->push_back(name);
        }
        records[name] = cur_name;
        cur_name.clear();
    }

    // Fills the database with zone data.
    // This method constructs a number of resource records (with addRecord),
    // which will all be added for one domain name to the fake database
    // (with addCurName). So for instance the first set of calls create
    // data for the name 'www.example.org', which will consist of one A RRset
    // of one record, and one AAAA RRset of two records.
    // The order in which they are added is the order in which getNextRecord()
    // will return them (so we can test whether find() etc. support data that
    // might not come in 'normal' order)
    // It shall immediately fail if you try to add the same name twice.
    void fillData() {
        // some plain data
        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("AAAA", "3600", "", "2001:db8::1");
        addRecord("AAAA", "3600", "", "2001:db8::2");
        addCurName("www.example.org.");

        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("AAAA", "3600", "", "2001:db8::1");
        addRecord("A", "3600", "", "192.0.2.2");
        addCurName("www2.example.org.");

        addRecord("CNAME", "3600", "", "www.example.org.");
        addCurName("cname.example.org.");

        // some DNSSEC-'signed' data
        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("RRSIG", "3600", "", "A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addRecord("RRSIG", "3600", "", "A 5 3 3600 20000101000000 20000201000000 12346 example.org. FAKEFAKEFAKE");
        addRecord("AAAA", "3600", "", "2001:db8::1");
        addRecord("AAAA", "3600", "", "2001:db8::2");
        addRecord("RRSIG", "3600", "", "AAAA 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addCurName("signed1.example.org.");
        addRecord("CNAME", "3600", "", "www.example.org.");
        addRecord("RRSIG", "3600", "", "CNAME 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addCurName("signedcname1.example.org.");
        // special case might fail; sig is for cname, which isn't there (should be ignored)
        // (ignoring of 'normal' other type is done above by www.)
        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("RRSIG", "3600", "", "A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addRecord("RRSIG", "3600", "", "CNAME 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addCurName("acnamesig1.example.org.");

        // let's pretend we have a database that is not careful
        // about the order in which it returns data
        addRecord("RRSIG", "3600", "", "A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addRecord("AAAA", "3600", "", "2001:db8::2");
        addRecord("RRSIG", "3600", "", "A 5 3 3600 20000101000000 20000201000000 12346 example.org. FAKEFAKEFAKE");
        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("RRSIG", "3600", "", "AAAA 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addRecord("AAAA", "3600", "", "2001:db8::1");
        addCurName("signed2.example.org.");
        addRecord("RRSIG", "3600", "", "CNAME 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addRecord("CNAME", "3600", "", "www.example.org.");
        addCurName("signedcname2.example.org.");

        addRecord("RRSIG", "3600", "", "CNAME 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("RRSIG", "3600", "", "A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addCurName("acnamesig2.example.org.");

        addRecord("RRSIG", "3600", "", "CNAME 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addRecord("RRSIG", "3600", "", "A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addRecord("A", "3600", "", "192.0.2.1");
        addCurName("acnamesig3.example.org.");

        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("A", "360", "", "192.0.2.2");
        addCurName("ttldiff1.example.org.");
        addRecord("A", "360", "", "192.0.2.1");
        addRecord("A", "3600", "", "192.0.2.2");
        addCurName("ttldiff2.example.org.");

        // also add some intentionally bad data
        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("CNAME", "3600", "", "www.example.org.");
        addCurName("badcname1.example.org.");

        addRecord("CNAME", "3600", "", "www.example.org.");
        addRecord("A", "3600", "", "192.0.2.1");
        addCurName("badcname2.example.org.");

        addRecord("CNAME", "3600", "", "www.example.org.");
        addRecord("CNAME", "3600", "", "www.example2.org.");
        addCurName("badcname3.example.org.");

        addRecord("A", "3600", "", "bad");
        addCurName("badrdata.example.org.");

        addRecord("BAD_TYPE", "3600", "", "192.0.2.1");
        addCurName("badtype.example.org.");

        addRecord("A", "badttl", "", "192.0.2.1");
        addCurName("badttl.example.org.");

        addRecord("A", "badttl", "", "192.0.2.1");
        addRecord("RRSIG", "3600", "", "A 5 3 3600 somebaddata 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addCurName("badsig.example.org.");

        addRecord("A", "3600", "", "192.0.2.1");
        addRecord("RRSIG", "3600", "TXT", "A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
        addCurName("badsigtype.example.org.");
    }
};

// This tests the default getAllRecords behaviour, throwing NotImplemented
TEST(DatabaseConnectionTest, getAllRecords) {
    // The parameters don't matter
    EXPECT_THROW(NopAccessor().getAllRecords(Name("."), 1),
                 isc::NotImplemented);
}

class DatabaseClientTest : public ::testing::Test {
public:
    DatabaseClientTest() {
        createClient();
    }
    /*
     * We initialize the client from a function, so we can call it multiple
     * times per test.
     */
    void createClient() {
        current_database_ = new MockAccessor();
        client_.reset(new DatabaseClient(shared_ptr<DatabaseAccessor>(
             current_database_)));
    }
    // Will be deleted by client_, just keep the current value for comparison.
    MockAccessor* current_database_;
    shared_ptr<DatabaseClient> client_;
    const std::string database_name_;

    /**
     * Check the zone finder is a valid one and references the zone ID and
     * database available here.
     */
    void checkZoneFinder(const DataSourceClient::FindResult& zone) {
        ASSERT_NE(ZoneFinderPtr(), zone.zone_finder) << "No zone finder";
        shared_ptr<DatabaseClient::Finder> finder(
            dynamic_pointer_cast<DatabaseClient::Finder>(zone.zone_finder));
        ASSERT_NE(shared_ptr<DatabaseClient::Finder>(), finder) <<
            "Wrong type of finder";
        EXPECT_EQ(42, finder->zone_id());
        EXPECT_EQ(current_database_, &finder->database());
    }
};

TEST_F(DatabaseClientTest, zoneNotFound) {
    DataSourceClient::FindResult zone(client_->findZone(Name("example.com")));
    EXPECT_EQ(result::NOTFOUND, zone.code);
}

TEST_F(DatabaseClientTest, exactZone) {
    DataSourceClient::FindResult zone(client_->findZone(Name("example.org")));
    EXPECT_EQ(result::SUCCESS, zone.code);
    checkZoneFinder(zone);
}

TEST_F(DatabaseClientTest, superZone) {
    DataSourceClient::FindResult zone(client_->findZone(Name(
        "sub.example.org")));
    EXPECT_EQ(result::PARTIALMATCH, zone.code);
    checkZoneFinder(zone);
}

TEST_F(DatabaseClientTest, noAccessorException) {
    // We need a dummy variable here; some compiler would regard it a mere
    // declaration instead of an instantiation and make the test fail.
    EXPECT_THROW(DatabaseClient dummy((shared_ptr<DatabaseAccessor>())),
                 isc::InvalidParameter);
}

// If the zone doesn't exist, exception is thrown
TEST_F(DatabaseClientTest, noZoneIterator) {
    EXPECT_THROW(client_->getIterator(Name("example.com")), DataSourceError);
}

// If the zone doesn't exist and iteration is not implemented, it still throws
// the exception it doesn't exist
TEST_F(DatabaseClientTest, noZoneNotImplementedIterator) {
    EXPECT_THROW(DatabaseClient(boost::shared_ptr<DatabaseAccessor>(
        new NopAccessor())).getIterator(Name("example.com")),
                 DataSourceError);
}

TEST_F(DatabaseClientTest, notImplementedIterator) {
    EXPECT_THROW(DatabaseClient(shared_ptr<DatabaseAccessor>(
        new NopAccessor())).getIterator(Name("example.org")),
                 isc::NotImplemented);
}

// Pretend a bug in the connection and pass NULL as the context
// Should not crash, but gracefully throw
TEST_F(DatabaseClientTest, nullIteratorContext) {
    EXPECT_THROW(client_->getIterator(Name("null.example.org")),
                 isc::Unexpected);
}

// It doesn't crash or anything if the zone is completely empty
TEST_F(DatabaseClientTest, emptyIterator) {
    ZoneIteratorPtr it(client_->getIterator(Name("empty.example.org")));
    EXPECT_EQ(ConstRRsetPtr(), it->getNextRRset());
    // This is past the end, it should throw
    EXPECT_THROW(it->getNextRRset(), isc::Unexpected);
}

// Iterate trough a zone
TEST_F(DatabaseClientTest, iterator) {
    ZoneIteratorPtr it(client_->getIterator(Name("example.org")));
    ConstRRsetPtr rrset(it->getNextRRset());
    ASSERT_NE(ConstRRsetPtr(), rrset);
    EXPECT_EQ(Name("example.org"), rrset->getName());
    EXPECT_EQ(RRClass::IN(), rrset->getClass());
    EXPECT_EQ(RRType::SOA(), rrset->getType());
    EXPECT_EQ(RRTTL(300), rrset->getTTL());
    RdataIteratorPtr rit(rrset->getRdataIterator());
    ASSERT_FALSE(rit->isLast());
    rit->next();
    EXPECT_TRUE(rit->isLast());

    rrset = it->getNextRRset();
    ASSERT_NE(ConstRRsetPtr(), rrset);
    EXPECT_EQ(Name("x.example.org"), rrset->getName());
    EXPECT_EQ(RRClass::IN(), rrset->getClass());
    EXPECT_EQ(RRType::A(), rrset->getType());
    EXPECT_EQ(RRTTL(300), rrset->getTTL());
    rit = rrset->getRdataIterator();
    ASSERT_FALSE(rit->isLast());
    EXPECT_EQ("192.0.2.1", rit->getCurrent().toText());
    rit->next();
    ASSERT_FALSE(rit->isLast());
    EXPECT_EQ("192.0.2.2", rit->getCurrent().toText());
    rit->next();
    EXPECT_TRUE(rit->isLast());

    rrset = it->getNextRRset();
    ASSERT_NE(ConstRRsetPtr(), rrset);
    EXPECT_EQ(Name("x.example.org"), rrset->getName());
    EXPECT_EQ(RRClass::IN(), rrset->getClass());
    EXPECT_EQ(RRType::AAAA(), rrset->getType());
    EXPECT_EQ(RRTTL(300), rrset->getTTL());
    EXPECT_EQ(ConstRRsetPtr(), it->getNextRRset());
    rit = rrset->getRdataIterator();
    ASSERT_FALSE(rit->isLast());
    EXPECT_EQ("2001:db8::1", rit->getCurrent().toText());
    rit->next();
    ASSERT_FALSE(rit->isLast());
    EXPECT_EQ("2001:db8::2", rit->getCurrent().toText());
    rit->next();
    EXPECT_TRUE(rit->isLast());
}

// This has inconsistent TTL in the set (the rest, like nonsense in
// the data is handled in rdata itself).
TEST_F(DatabaseClientTest, badIterator) {
    // It should not throw, but get the lowest one of them
    ZoneIteratorPtr it(client_->getIterator(Name("bad.example.org")));
    EXPECT_EQ(it->getNextRRset()->getTTL(), isc::dns::RRTTL(300));
}

// checks if the given rrset matches the
// given name, class, type and rdatas
void
checkRRset(isc::dns::ConstRRsetPtr rrset,
           const isc::dns::Name& name,
           const isc::dns::RRClass& rrclass,
           const isc::dns::RRType& rrtype,
           const isc::dns::RRTTL& rrttl,
           const std::vector<std::string>& rdatas) {
    isc::dns::RRsetPtr expected_rrset(
        new isc::dns::RRset(name, rrclass, rrtype, rrttl));
    for (unsigned int i = 0; i < rdatas.size(); ++i) {
        expected_rrset->addRdata(
            isc::dns::rdata::createRdata(rrtype, rrclass,
                                         rdatas[i]));
    }
    isc::testutils::rrsetCheck(expected_rrset, rrset);
}

void
doFindTest(shared_ptr<DatabaseClient::Finder> finder,
           const isc::dns::Name& name,
           const isc::dns::RRType& type,
           const isc::dns::RRType& expected_type,
           const isc::dns::RRTTL expected_ttl,
           ZoneFinder::Result expected_result,
           const std::vector<std::string>& expected_rdatas,
           const std::vector<std::string>& expected_sig_rdatas)
{
    ZoneFinder::FindResult result =
        finder->find(name, type, NULL, ZoneFinder::FIND_DEFAULT);
    ASSERT_EQ(expected_result, result.code) << name << " " << type;
    if (expected_rdatas.size() > 0) {
        checkRRset(result.rrset, name, finder->getClass(),
                   expected_type, expected_ttl, expected_rdatas);

        if (expected_sig_rdatas.size() > 0) {
            checkRRset(result.rrset->getRRsig(), name,
                       finder->getClass(), isc::dns::RRType::RRSIG(),
                       expected_ttl, expected_sig_rdatas);
        } else {
            EXPECT_EQ(isc::dns::RRsetPtr(), result.rrset->getRRsig());
        }
    } else {
        EXPECT_EQ(isc::dns::RRsetPtr(), result.rrset);
    }
}

TEST_F(DatabaseClientTest, find) {
    DataSourceClient::FindResult zone(client_->findZone(Name("example.org")));
    ASSERT_EQ(result::SUCCESS, zone.code);
    shared_ptr<DatabaseClient::Finder> finder(
        dynamic_pointer_cast<DatabaseClient::Finder>(zone.zone_finder));
    EXPECT_EQ(42, finder->zone_id());
    EXPECT_FALSE(current_database_->searchRunning());
    std::vector<std::string> expected_rdatas;
    std::vector<std::string> expected_sig_rdatas;

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    doFindTest(finder, isc::dns::Name("www.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_rdatas.push_back("192.0.2.2");
    doFindTest(finder, isc::dns::Name("www2.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("2001:db8::1");
    expected_rdatas.push_back("2001:db8::2");
    doFindTest(finder, isc::dns::Name("www.example.org."),
               isc::dns::RRType::AAAA(), isc::dns::RRType::AAAA(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    doFindTest(finder, isc::dns::Name("www.example.org."),
               isc::dns::RRType::TXT(), isc::dns::RRType::TXT(),
               isc::dns::RRTTL(3600),
               ZoneFinder::NXRRSET,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("www.example.org.");
    doFindTest(finder, isc::dns::Name("cname.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::CNAME(),
               isc::dns::RRTTL(3600),
               ZoneFinder::CNAME,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("www.example.org.");
    doFindTest(finder, isc::dns::Name("cname.example.org."),
               isc::dns::RRType::CNAME(), isc::dns::RRType::CNAME(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    doFindTest(finder, isc::dns::Name("doesnotexist.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::NXDOMAIN,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_sig_rdatas.push_back("A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    expected_sig_rdatas.push_back("A 5 3 3600 20000101000000 20000201000000 12346 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("signed1.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("2001:db8::1");
    expected_rdatas.push_back("2001:db8::2");
    expected_sig_rdatas.push_back("AAAA 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("signed1.example.org."),
               isc::dns::RRType::AAAA(), isc::dns::RRType::AAAA(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    doFindTest(finder, isc::dns::Name("signed1.example.org."),
               isc::dns::RRType::TXT(), isc::dns::RRType::TXT(),
               isc::dns::RRTTL(3600),
               ZoneFinder::NXRRSET,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("www.example.org.");
    expected_sig_rdatas.push_back("CNAME 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("signedcname1.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::CNAME(),
               isc::dns::RRTTL(3600),
               ZoneFinder::CNAME,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_sig_rdatas.push_back("A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    expected_sig_rdatas.push_back("A 5 3 3600 20000101000000 20000201000000 12346 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("signed2.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("2001:db8::2");
    expected_rdatas.push_back("2001:db8::1");
    expected_sig_rdatas.push_back("AAAA 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("signed2.example.org."),
               isc::dns::RRType::AAAA(), isc::dns::RRType::AAAA(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    doFindTest(finder, isc::dns::Name("signed2.example.org."),
               isc::dns::RRType::TXT(), isc::dns::RRType::TXT(),
               isc::dns::RRTTL(3600),
               ZoneFinder::NXRRSET,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("www.example.org.");
    expected_sig_rdatas.push_back("CNAME 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("signedcname2.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::CNAME(),
               isc::dns::RRTTL(3600),
               ZoneFinder::CNAME,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());


    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_sig_rdatas.push_back("A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("acnamesig1.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_sig_rdatas.push_back("A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("acnamesig2.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_sig_rdatas.push_back("A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("acnamesig3.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_rdatas.push_back("192.0.2.2");
    doFindTest(finder, isc::dns::Name("ttldiff1.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(360),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());

    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_rdatas.push_back("192.0.2.2");
    doFindTest(finder, isc::dns::Name("ttldiff2.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(360),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());


    EXPECT_THROW(finder->find(isc::dns::Name("badcname1.example.org."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("badcname2.example.org."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("badcname3.example.org."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("badrdata.example.org."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("badtype.example.org."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("badttl.example.org."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("badsig.example.org."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());

    // Trigger the hardcoded exceptions and see if find() has cleaned up
    EXPECT_THROW(finder->find(isc::dns::Name("dsexception.in.search."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("iscexception.in.search."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("basicexception.in.search."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 std::exception);
    EXPECT_FALSE(current_database_->searchRunning());

    EXPECT_THROW(finder->find(isc::dns::Name("dsexception.in.getnext."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("iscexception.in.getnext."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 DataSourceError);
    EXPECT_FALSE(current_database_->searchRunning());
    EXPECT_THROW(finder->find(isc::dns::Name("basicexception.in.getnext."),
                                              isc::dns::RRType::A(),
                                              NULL, ZoneFinder::FIND_DEFAULT),
                 std::exception);
    EXPECT_FALSE(current_database_->searchRunning());

    // This RRSIG has the wrong sigtype field, which should be
    // an error if we decide to keep using that field
    // Right now the field is ignored, so it does not error
    expected_rdatas.clear();
    expected_sig_rdatas.clear();
    expected_rdatas.push_back("192.0.2.1");
    expected_sig_rdatas.push_back("A 5 3 3600 20000101000000 20000201000000 12345 example.org. FAKEFAKEFAKE");
    doFindTest(finder, isc::dns::Name("badsigtype.example.org."),
               isc::dns::RRType::A(), isc::dns::RRType::A(),
               isc::dns::RRTTL(3600),
               ZoneFinder::SUCCESS,
               expected_rdatas, expected_sig_rdatas);
    EXPECT_FALSE(current_database_->searchRunning());
}

}
