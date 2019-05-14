#include <time.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <limits>
#include "tendisplus/storage/record.h"
#include "tendisplus/utils/invariant.h"
#include "tendisplus/utils/string.h"
#include "gtest/gtest.h"

namespace tendisplus {

static int genRand() {
    static int grand = 0;
    grand = rand_r(reinterpret_cast<unsigned int *>(&grand));
    return grand;
}

RecordType randomType() {
    switch ((genRand() % 13)) {
        case 0:
            return RecordType::RT_META;
        case 1:
            return RecordType::RT_KV;
        case 2:
            return RecordType::RT_LIST_META;
        case 3:
            return RecordType::RT_LIST_ELE;
        case 4:
            return RecordType::RT_HASH_ELE;
        case 5:
            return RecordType::RT_HASH_META;
        case 6:
            return RecordType::RT_SET_ELE;
        case 7:
            return RecordType::RT_SET_META;
        case 8:
            return RecordType::RT_ZSET_H_ELE;
        case 9:
            return RecordType::RT_ZSET_S_ELE;
        case 10:
            return RecordType::RT_ZSET_META;
        case 11:
            return RecordType::RT_TTL_INDEX;
        case 12:
            return RecordType::RT_BINLOG;
        default:
            return RecordType::RT_INVALID;
    }
}

ReplFlag randomReplFlag() {
    switch ((genRand() % 3)) {
        case 0:
            return ReplFlag::REPL_GROUP_MID;
        case 1:
            return ReplFlag::REPL_GROUP_START;
        case 2:
            return ReplFlag::REPL_GROUP_END;
        default:
            INVARIANT(0);
            // void compiler complain
            return ReplFlag::REPL_GROUP_MID;
    }
}

ReplOp randomReplOp() {
    switch ((genRand() % 3)) {
        case 0:
            return ReplOp::REPL_OP_NONE;
        case 1:
            return ReplOp::REPL_OP_SET;
        case 2:
            return ReplOp::REPL_OP_DEL;
        default:
            INVARIANT(0);
            // void compiler complain
            return ReplOp::REPL_OP_NONE;
    }
}

std::string randomStr(size_t s, bool maybeEmpty) {
    if (s == 0) {
        s = genRand() % 256;
    }
    if (!maybeEmpty) {
        s++;
    }
    std::vector<uint8_t> v;
    for (size_t i = 0; i < s; i++) {
        v.emplace_back(genRand() % 256);
    }
    return std::string(reinterpret_cast<const char*>(v.data()), v.size());
}

std::string overflip(const std::string& s) {
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), s.begin(), s.end());
    size_t size = buf.size();
    auto ori1 = buf[size-2];
    while (ori1 == buf[size-2]) {
        buf[size-2] = genRand() % 256;
    }
    return std::string(
        reinterpret_cast<const char *>(buf.data()), buf.size());
}

TEST(Record, Common) {
    srand((unsigned int)time(NULL));
    for (size_t i = 0; i < 1000000; i++) {
        uint32_t dbid = genRand();
        uint32_t chunkid = genRand();
        auto type = randomType();
        auto pk = randomStr(5, false);
        auto sk = randomStr(5, true);
        uint64_t ttl = genRand()*genRand();
        uint64_t cas = genRand()*genRand();
        uint64_t version = genRand()*genRand();
        uint64_t versionEP = genRand()*genRand();
        auto val = randomStr(5, true);
        uint64_t pieceSize = (uint64_t)-1;
        if (val.size() % 2 == 0) {
            pieceSize = val.size() + 1;
        }
        auto rk = RecordKey(chunkid, dbid, type, pk, sk);
        auto rv = RecordValue(val, type, ttl, cas, version, versionEP, pieceSize);
        auto rcd = Record(rk, rv);
        auto kv = rcd.encode();
        auto prcd1 = Record::decode(kv.first, kv.second);
        auto type_ = RecordKey::getRecordTypeRaw(kv.first.c_str(), 
            kv.first.size());
        auto ttl_ = RecordValue::getTtlRaw(kv.second.c_str(), 
            kv.second.size());
        
        EXPECT_EQ(cas, rv.getCas());
        EXPECT_EQ(version, rv.getVersion());
        EXPECT_EQ(versionEP, rv.getVersionEP());
        EXPECT_EQ(pieceSize, rv.getPieceSize());
        EXPECT_EQ(type, rv.getRecordType());
        EXPECT_EQ(ttl, rv.getTtl());

        EXPECT_EQ(getRealKeyType(type), rk.getRecordType());
        EXPECT_EQ(type_, getRealKeyType(type));
        EXPECT_EQ(ttl_, ttl);
        EXPECT_TRUE(prcd1.ok());
        EXPECT_EQ(prcd1.value(), rcd);
    }

    //for (size_t i = 0; i < 1000000; i++) {
    //    uint32_t dbid = genRand();
    //    uint32_t chunkid = genRand();
    //    auto type = randomType();
    //    auto pk = randomStr(5, false);
    //    auto sk = randomStr(5, true);
    //    uint64_t ttl = genRand()*genRand();
    //    uint64_t cas = genRand()*genRand();
    //    auto val = randomStr(5, true);
    //    auto rk = RecordKey(chunkid, dbid, type, pk, sk);
    //    auto rv = RecordValue(val, ttl, cas);
    //    auto rcd = Record(rk, rv);
    //    auto kv = rcd.encode();
    //    auto prcd1 = Record::decode(overflip(kv.first), kv.second);
    //    EXPECT_TRUE(
    //        prcd1.status().code() == ErrorCodes::ERR_DECODE ||
    //        !(prcd1.value().getRecordKey() == rk));
    //}
}

TEST(ReplRecord, Prefix) {
    uint64_t timestamp = (uint64_t)genRand() + std::numeric_limits<uint32_t>::max();
    auto rlk = ReplLogKey(genRand(), 0, randomReplFlag(), timestamp);
    RecordKey rk(ReplLogKey::CHUNKID, ReplLogKey::DBID,
                 RecordType::RT_BINLOG, rlk.encode(), "");
    const std::string s = rk.encode();
    EXPECT_EQ(s[0], '\xff');
    EXPECT_EQ(s[1], '\xff');
    EXPECT_EQ(s[2], '\xff');
    EXPECT_EQ(s[3], '\xff');
    EXPECT_EQ(s[4], '\xff');
    EXPECT_EQ(s[5], '\xff');
    EXPECT_EQ(s[6], '\xff');
    EXPECT_EQ(s[7], '\xff');
    EXPECT_EQ(s[8], '\xff');
    const std::string& prefix = RecordKey::prefixReplLog();
    for (int i = 0; i < 100000; ++i) {
        EXPECT_TRUE(randomStr(5, false) <= prefix);
    }
}

TEST(ReplRecord, Common) {
    srand(time(NULL));
    std::vector<ReplLogKey> logKeys;
    for (size_t i = 0; i < 100000; i++) {
        uint64_t txnid = uint64_t(genRand())*uint64_t(genRand());
        uint16_t localid = uint16_t(genRand());
        ReplFlag flag = randomReplFlag();
        uint64_t timestamp = (uint64_t)genRand()+std::numeric_limits<uint32_t>::max();
        auto rk = ReplLogKey(txnid, localid, flag, timestamp);
        auto rkStr = rk.encode();
        auto prk = ReplLogKey::decode(rkStr);
        EXPECT_TRUE(prk.ok());
        EXPECT_EQ(prk.value(), rk);
        logKeys.emplace_back(std::move(rk));
    }
    std::sort(logKeys.begin(), logKeys.end(),
        [](const ReplLogKey& a, const ReplLogKey& b) {
            return a.encode() < b.encode();
        });
    for (size_t i = 0; i < logKeys.size()-1; ++i) {
        EXPECT_TRUE(logKeys[i].getTxnId() < logKeys[i+1].getTxnId()
            ||(logKeys[i].getTxnId() == logKeys[i+1].getTxnId() &&
            logKeys[i].getLocalId() <= logKeys[i+1].getLocalId()));
    }

    ReplLogValue rlv(ReplOp::REPL_OP_SET, "a", "b");
    std::string s = rlv.encode();
    Expected<ReplLogValue> erlv = ReplLogValue::decode(s);
    EXPECT_TRUE(erlv.ok());
}

TEST(ZSl, Common) {
    srand(time(NULL));
    for (size_t i = 0; i < 1000000; i++) {
        // uint8_t maxLvl = genRand() % std::numeric_limits<uint8_t>::max();
        uint8_t maxLvl = ZSlMetaValue::MAX_LAYER;
        uint8_t lvl = genRand() % maxLvl;
        uint32_t count = static_cast<uint32_t>(genRand());
        uint64_t tail = static_cast<uint64_t>(genRand()) * genRand();
        ZSlMetaValue m(lvl, count, tail);
        EXPECT_EQ(m.getMaxLevel(), maxLvl);
        EXPECT_EQ(m.getLevel(), lvl);
        EXPECT_EQ(m.getCount(), count);
        EXPECT_EQ(m.getTail(), tail);
        std::string s = m.encode();
        Expected<ZSlMetaValue> expm = ZSlMetaValue::decode(s);
        EXPECT_TRUE(expm.ok());
        EXPECT_EQ(expm.value().getMaxLevel(), maxLvl);
        EXPECT_EQ(expm.value().getLevel(), lvl);
        EXPECT_EQ(expm.value().getCount(), count);
        EXPECT_EQ(expm.value().getTail(), tail);
    }

    for (size_t i = 0; i < 1000000; i++) {
        ZSlEleValue v(genRand(), randomStr(256, false));
        for (uint8_t i = 1; i <= ZSlMetaValue::MAX_LAYER; ++i) {
            v.setForward(i, genRand());
            v.setSpan(i, genRand());
        }
        std::string s = v.encode();
        Expected<ZSlEleValue> expv = ZSlEleValue::decode(s);
        EXPECT_TRUE(expv.ok());
        for (uint8_t i = 1; i <= ZSlMetaValue::MAX_LAYER; ++i) {
            EXPECT_EQ(expv.value().getForward(i), v.getForward(i));
            EXPECT_EQ(expv.value().getSpan(i), v.getSpan(i));
        }
        EXPECT_EQ(expv.value().getScore(), v.getScore());
        EXPECT_EQ(expv.value().getSubKey(), v.getSubKey());
    }
}

}  // namespace tendisplus