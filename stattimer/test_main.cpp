#include <string>
#include <sstream>
#include <iomanip>
#include "stattimer.hpp"


class TestBase {
public:
    void testGroup(std::string name) {
        group_name = name;
    }
    template<typename T>
    void expectEq(const std::string& msg, const T& a, const T& b) {
        if (a == b) {
            return;
        }
        std::cout << "FAIL: [" << group_name << "] " << msg << std::endl;
        std::cout << "expected: " << a << std::endl;
        std::cout << "actual:   " << b << std::endl;
    }
    template<typename T>
    void expectEq(const std::string& msg,
                         const std::vector<T>& a, const std::vector<T>& b) {
        if (a == b) {
            return;
        }
        std::cout << "FAIL: [" << group_name << "] " << msg << std::endl;
    }
    template<typename T>
    void expectNeq(const std::string& msg, const T& a, const T& b) {
        if (a != b) {
            return;
        }
        std::cout << "FAIL: [" << group_name << "] " << msg << std::endl;
        std::cout << "!expected: " << a << std::endl;
        std::cout << "actual:    " << b << std::endl;
    }
    template<typename T>
    void expectNeq(const std::string& msg,
                          const std::vector<T>& a, const std::vector<T>& b) {
        if (a != b) {
            return;
        }
        std::cout << "FAIL: [" << group_name << "] " << msg << std::endl;
    }
protected:
    std::string group_name;
};


class TestStatTimer : public TestBase {
public:
    class STimerMock : public STimer {
        friend class STimerList_<STimerMock>;
    public:
        static double tick;
        static double tick_period;
    private:
        void start() {
            start_count = tickCount();
            is_recording = true;
        }
        void start(double start_count_value) {
            start_count = start_count_value;
            is_recording = true;
        }
        double stop() {
            double stop_count = tickCount();
            double delta = (stop_count - start_count) * tickPeriod();
            registerTime(delta);
            return stop_count;
        }
        static double tickCount() { return tick; }
        static double tickPeriod() { return tick_period; }
    };

    typedef STimerList_<STimerMock> STimerListMock;
    typedef STimerScoped_<STimerMock> STimerScopedMock;


    STimerListMock st;
    TestStatTimer() : st(5) {}

    static std::string myReporterCSV(STimerRecords& rec) {
        std::ostringstream stream;
        stream << rec.id << ", "
               << rec.label << ", "
               << std::fixed << std::setprecision(3)
               << rec.mean << ", "
               << rec.stddev << ", "
               << rec.maximum << ", "
               << rec.minimum << ", "
               << rec.nsample << std::endl;
        return stream.str();
    }

    void printFunc() {
        STimerScopedMock stp(st, "printFunc scoped");
        STimerMock::tick += 5;
    }

    int testMain() {
        using namespace std;

        STimerMock::tick_period = 1.0;
        STimerMock::tick = 0.0;

        //=======================================================
        testGroup("Typical Usage");
        for (int i = 0; i < 10; i++) {
            st.laptime("for laptime");
            {
                STimerScopedMock sts(st, "for scoped");
                STimerMock::tick += 1;

                st.start("printFunc");
                printFunc();
                STimerMock::tick += i;
                st.stop("printFunc");
            }
        }

        STimerRecords rec;
        //-----------
        rec = st.calcStat("for scoped");
        double m_gt = (6 + 15) / 2.0;
        expectEq("for scoped mean", m_gt, rec.mean);

        double s_gt = 0.0;
        for (int i = 0; i < 10; i++) {
            s_gt += (6 + i) * (6 + i);
        }
        s_gt /= 10.0;
        s_gt -= m_gt * m_gt;
        s_gt = sqrt(s_gt);
        expectEq("for scoped stddev", s_gt, rec.stddev);
        expectEq("for scoped maximum", 15.0, rec.maximum);
        expectEq("for scoped minimum", 6.0, rec.minimum);

        //-----------
        rec = st.calcStat("for laptime");
        m_gt = (6 + 14) / 2.0;
        expectEq("for laptime mean", m_gt, rec.mean);
        s_gt = 0.0;
        for (int i = 0; i < 9; i++) {
            s_gt += (6 + i) * (6 + i);
        }
        s_gt /= 9;
        s_gt -= m_gt * m_gt;
        s_gt = sqrt(s_gt);
        expectEq("for laptime stddev", s_gt, rec.stddev);
        expectEq("for laptime maximum", 14.0, rec.maximum);
        expectEq("for laptime minimum", 6.0, rec.minimum);

        //-----------
        rec = st.calcStat("printFunc");
        m_gt = (5 + 14) / 2.0;
        expectEq("for printFunc mean", m_gt, rec.mean);
        s_gt = 0.0;
        for (int i = 0; i < 10; i++) {
            s_gt += (5 + i) * (5 + i);
        }
        s_gt /= 10;
        s_gt -= m_gt * m_gt;
        s_gt = sqrt(s_gt);
        expectEq("for printFunc stddev", s_gt, rec.stddev);
        expectEq("for printFunc maximum", 14.0, rec.maximum);
        expectEq("for printFunc minimum", 5.0, rec.minimum);

        //-----------
        rec = st.calcStat("printFunc scoped");
        expectEq("for printFunc scoped mean", 5.0, rec.mean);
        expectEq("for printFunc scoped stddev", 0.0, rec.stddev);
        expectEq("for printFunc scoped maximum", 5.0, rec.maximum);
        expectEq("for printFunc scoped minimum", 5.0, rec.minimum);

        //=======================================================
        // multiple labels set for a single timer (the newest survives)
        testGroup("Multi Labels");

        st.setLabel(10, "label1");
        st.setLabel(10, "label2");
        st.setLabel(10, "label3");
        st.start(10);
        STimerMock::tick += 1;
        st.stop(10);
        rec = st.calcStat(10);
        expectEq("lookup by num", (string)"label3", rec.label);
        rec = st.calcStat("label3");
        expectEq("lookup by label3", 1.0, rec.mean);
        rec = st.calcStat("label1");
        expectEq("lookup by label1", 0, rec.nsample);

        //=======================================================
        // the same label set for multiple timers
        // (the newest survises, and the other labels are cleared)
        testGroup("Duplicated Label");
        st.setLabel(22, "label4");
        st.setLabel(21, "label4");
        st.setLabel(20, "label4");
        st.start(20);
        STimerMock::tick += 20;
        st.stop(20);
        st.start(21);
        STimerMock::tick += 21;
        st.stop(21);
        st.start(22);
        STimerMock::tick += 22;
        st.stop(22);
        rec = st.calcStat("label4");
        expectEq("lookup by label4", 20.0, rec.mean);
        rec = st.calcStat(21);
        expectEq("lookup by 21", 21.0, rec.mean);

        //=======================================================
        // reporterFunc temporarily applied to a timer
        testGroup("Temporary Reporter");

        string rep_gt = myReporterCSV(st.calcStat("label4"));
        string rep = st.report("label4", myReporterCSV);
        expectEq("called with func", rep_gt, rep);
        rep = st.report("label4");
        expectNeq("called w/o func", rep_gt, rep);

        //=======================================================
        // reporterFunc set for a timer
        testGroup("Reporter Set");

        st.setReporterFunc("label4", myReporterCSV);
        rep_gt = myReporterCSV(st.calcStat("label4"));
        rep = st.report("label4");
        expectEq("set", rep_gt, rep);
        rep_gt = myReporterCSV(st.calcStat(21));
        rep = st.report(21);
        expectNeq("another timer affected", rep_gt, rep);

        //=======================================================
        // reporterFunc set for all the timers
        testGroup("Reporter Set for All");

        st.setReporterFunc(myReporterCSV);
        rep_gt = myReporterCSV(st.calcStat(21));
        rep = st.report(21);
        expectEq("21", rep_gt, rep);
        rep_gt = myReporterCSV(st.calcStat(22));
        rep = st.report(22);
        expectEq("22", rep_gt, rep);

        //=======================================================
        // time recording buffer
        testGroup("timebuf");

        st.setLabel(30, "buftest");
        st.initTimeBuf("buftest", 5);
        for (int i = 0; i < 3; i++) {
            STimerScopedMock s0(st, "buftest");
            STimerMock::tick += i;
        }
        rec = st.calcStat("buftest");
        expectEq("shorter seq", (size_t)3, rec.timebuf.size());

        vector<double> tbuf_gt;
        for (int i = 5; i < 10; i++) {
            STimerScopedMock s0(st, "buftest");
            STimerMock::tick += i;
            tbuf_gt.push_back(i);
        }
        rec = st.calcStat("buftest");
        expectEq("longer seq", tbuf_gt, rec.timebuf);


        // set the default reporter again
        st.setReporterFunc(STimerListMock::reporterDefault);

        // supress reporting in destructor
        st.setReporterFunc(NULL);

        return 0;
    }
};

double TestStatTimer::STimerMock::tick;
double TestStatTimer::STimerMock::tick_period;

int
main()
{
    TestStatTimer tester;
    tester.testMain();
    std::cout << "*** test done" << std::endl;
    return 0;
}
