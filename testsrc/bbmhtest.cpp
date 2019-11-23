#include "bbmh.h"
#include "hll.h"
using namespace sketch;
using namespace common;
using namespace mh;

#ifndef SIMPLE_HASH
#define SIMPLE_HASH 1
#endif

template<typename T>
struct scope_executor {
    T x_;
    scope_executor(T &&x): x_(std::move(x)) {}
    scope_executor(const T &x): x_(x) {}
    ~scope_executor() {x_();}
};

int main() {
    static_assert(sizeof(schism::Schismatic<int32_t>) == sizeof(schism::Schismatic<uint32_t>), "wrong size!");

    {
        BBitMinHasher<uint64_t> b1(10, 4), b2(10, 4);
        b1.addh(1);
        b1.addh(4);
        b1.addh(137);

        b2.addh(1);
        b2.addh(4);
        b2.addh(17);
        auto f1 = b1.cfinalize(), f2 = b2.cfinalize();
        std::fprintf(stderr, "f1 popcount: %" PRIu64 "\n", f1.popcnt());
        std::fprintf(stderr, "f2 popcount: %" PRIu64 "\n", f2.popcnt());
        b1.show();
        b2.show();
        auto b3 = b1 + b2;
        b3.show();
        auto f3 = b3.finalize();
        std::fprintf(stderr, "f3 popcount: %" PRIu64 "\n", f3.popcnt());
        auto neqb12 = f1.equal_bblocks(f2);
        std::fprintf(stderr, "eqb: %zu. With itself: %zu\n", size_t(neqb12), size_t(f1.equal_bblocks(f1)));
    }

    for(size_t i = 7; i <= 14; i += 2) {
        for(const auto b: {7u, 13u, 14u, 17u, 9u}) {
            std::fprintf(stderr, "b: %u. i: %zu\n", b, i);
            SuperMinHash<policy::SizePow2Policy> smhp2(1 << i);
            SuperMinHash<policy::SizeDivPolicy>  smhdp(1 << i);
            SuperMinHash<policy::SizePow2Policy> smhp21(1 << i);
            SuperMinHash<policy::SizeDivPolicy>  smhdp1(1 << i);
            hll::hll_t h1(i), h2(i);
            uint64_t seed = h1.hash(h1.hash(i) ^ h1.hash(b));
#if SIMPLE_HASH
            using HasherType = hash::WangHash;
#else
            using HasherType = hash::MultiplyAddXoRotNVec<33>;
#endif
            BBitMinHasher<uint64_t, HasherType> b1(i, b, 1, seed), b2(i, b, 1, seed), b3(i, b, 1, seed);
            size_t dbval = 1.5 * (size_t(1) << i);
            DivBBitMinHasher<uint64_t> db1(dbval, b), db2(dbval, b), db3(dbval, b);
            //DivBBitMinHasher<uint64_t> fb(i, b);
            CountingBBitMinHasher<uint64_t, uint32_t> cb1(i, b), cb2(i, b), cb3(i, b);
            DefaultRNGType gen(137 + (i * b));
            size_t shared = 0, b1c = 0, b2c = 0;
            constexpr size_t niter = 5000000;
            for(size_t i = niter; --i;) {
#if SIMPLE_HASH
                auto v = i;
#else
                auto v = gen();
#endif
                switch(v & 0x3uL) {
                    case 0:
                    case 1: h1.addh(v); h2.addh(v);
                            b2.addh(v); b1.addh(v); ++shared;
                            b3.addh(v);
                            db1.addh(v); db2.addh(v);
                            smhp2.addh(v); smhp21.addh(v);
                            smhdp.addh(v); smhdp1.addh(v);
                    /*fb.addh(v);*/
                    break;
                    case 2: h1.addh(v); b1.addh(v); ++b1c; b3.addh(v); cb3.addh(v); db1.addh(v);
                            smhp2.addh(v);
                            smhdp.addh(v);
                    break;
                    case 3: h2.addh(v); b2.addh(v); ++b2c; cb1.addh(v); db2.addh(v);
                            smhdp1.addh(v);
                            smhp21.addh(v);
                    break;
                }
                //if(i % 250000 == 0) std::fprintf(stderr, "%zu iterations left\n", size_t(i));
            }
            b1.densify();
            b2.densify();
            auto f1 = b1.finalize(), f2 = b2.finalize(), f3 = b3.finalize();
            auto est = (b1 + b2).cardinality_estimate();
            assert((b1 + b2).cardinality_estimate() == b1.union_size(b2));
            assert(i <= 9 || std::abs(est - niter < niter * 5 / 100.) || !std::fprintf(stderr, "est: %lf\n", est));
            //b1 += b2;
            auto f12 = b1.finalize();
            auto fdb1 = db1.finalize();
            auto fdb2 = db2.finalize();
            auto smh1 = smhp2.finalize(16), smh2 = smhp21.finalize(16);
            auto smhd1 = smhdp.finalize(16), smhd2 = smhdp1.finalize(16);
            assert(smh1.jaccard_index(smh1) == 1.);
            std::fprintf(stderr, "estimate: %f\n", smh1.jaccard_index(smh2));
            assert(std::abs(smh1.jaccard_index(smh2) - .5) < 0.05);

            std::fprintf(stderr, "with ss=%zu, smh1 and itself: %lf. 2 and 2/1 jaccard? %lf/%lf\n", size_t(1) << i, double(smh1.jaccard_index(smh1)), double(smh2.jaccard_index(smh1)), smh1.jaccard_index(smh2));
            std::fprintf(stderr, "smh1 card %lf, smh2 %lf\n", smh1.est_cardinality_, smh2.est_cardinality_);
            std::fprintf(stderr, "with ss=%zu, smhd1 and itself: %lf. 2 and 2/1 jaccard? %lf/%lf\n", size_t(1) << i, double(smhd1.jaccard_index(smhd1)), double(smhd2.jaccard_index(smhd1)), smhd1.jaccard_index(smhd2));
            std::fprintf(stderr, "Expected Cardinality [shared:%zu/b1:%zu/b2:%zu]\n", shared, b1c, b2c);
            std::fprintf(stderr, "h1 est %lf, h2 est: %lf\n", h1.report(), h2.report());
            std::fprintf(stderr, "Estimate Harmonicard [b1:%lf/b2:%lf]\n", b1.cardinality_estimate(HARMONIC_MEAN), b2.cardinality_estimate(HARMONIC_MEAN));
            std::fprintf(stderr, "Estimate div Harmonicard [b1:%lf/b2:%lf]\n", db1.cardinality_estimate(HARMONIC_MEAN), db2.cardinality_estimate(HARMONIC_MEAN));
            std::fprintf(stderr, "Estimate HLL [b1:%lf/b2:%lf/b3:%lf]\n", b1.cardinality_estimate(HLL_METHOD), b2.cardinality_estimate(HLL_METHOD), b3.cardinality_estimate(HLL_METHOD));
            std::fprintf(stderr, "Estimate arithmetic mean [b1:%lf/b2:%lf]\n", b1.cardinality_estimate(ARITHMETIC_MEAN), b2.cardinality_estimate(ARITHMETIC_MEAN));
            std::fprintf(stderr, "Estimate (median) b1:%lf/b2:%lf]\n", b1.cardinality_estimate(MEDIAN), b2.cardinality_estimate(MEDIAN));
            std::fprintf(stderr, "Estimate geometic mean [b1:%lf/b2:%lf]\n", b1.cardinality_estimate(GEOMETRIC_MEAN), b2.cardinality_estimate(GEOMETRIC_MEAN));
            std::fprintf(stderr, "JI for f3 and f2: %lf\n", f1.jaccard_index(f2));
            std::fprintf(stderr, "JI for fdb1 and fdb2: %lf, where nmin = %zu and b = %d\n", fdb2.jaccard_index(fdb1), i, b);
            //std::fprintf(stderr, "equal blocks: %zu\n", size_t(f2.equal_bblocks(f3)));
            std::fprintf(stderr, "f1, f2, and f3 cardinalities: %lf, %lf, %lf\n", f1.est_cardinality_, f2.est_cardinality_, f3.est_cardinality_);
            auto fcb1 = cb1.finalize(), fcb2 = cb3.finalize();
            //auto cb13res = fcb1.histogram_sums(fcb2);
            //assert(sizeof(cb13res) == sizeof(uint64_t) * 4);
            //std::fprintf(stderr, "cb13res %lf, %lf\n", cb13res.weighted_jaccard_index(), cb13res.jaccard_index());
            cb1.finalize().write("ZOMG.cb");
            decltype(cb1.finalize()) cbr("ZOMG.cb");
            auto deleter = []() {if(std::system("rm ZOMG.cb")) throw std::runtime_error("Failed to delete ZOMG.cb");};
            scope_executor<decltype(deleter)> se(deleter);
            assert(cbr == cb1.finalize());
            //cbr.histogram_sums(cb2.finalize()).print();
            auto whl = b1.make_whll();
            auto phl = b1.make_packed16hll();
            std::fprintf(stderr, "p16 card: %lf\n", phl.cardinality_estimate());
            std::fprintf(stderr, "whl card: %lf/%zu vs expected %lf/%lf/%lf\n", whl.cardinality_estimate(), whl.core_.size(), f1.est_cardinality_, h1.report(), whl.union_size(whl));
        }
    }
}
