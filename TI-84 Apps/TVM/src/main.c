#include <ti/screen.h>
#include <ti/getcsc.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#define EPS 1e-13L

static void heading(const char *title) {
    os_ClrHome();
    os_PutStrFull(title);
    os_NewLine();
    os_PutStrFull("-------------------------");
    os_NewLine();
}

static void line(const char *s) {
    os_PutStrFull(s);
    os_NewLine();
}

static void wait_key(void) {
    line("");
    line("PRESS ANY KEY: continue");
    while (os_GetCSC()) {}
    while (!os_GetCSC()) {}
    while (os_GetCSC()) {}
}

static long double number(const char *title, const char *prompt) {
    char buf[24];
    heading(title);
    os_GetStringInput(prompt, buf, sizeof buf);
    return strtold(buf, NULL);
}

static int frequency(const char *title) {
    char buf[12];
    heading(title);
    line("Monthly problem: enter 12");
    line("Quarterly problem: enter 4");
    line("Annual problem: enter 1");
    os_GetStringInput("Periods per year: ", buf, sizeof buf);
    long double x = strtold(buf, NULL);
    return x > 0 ? (int)(x + 0.5) : 0;
}

static int period_count(const char *title) {
    char buf[12];
    heading(title);
    line("Use total periods.");
    line("2 years monthly = 24");
    line("5 years quarterly = 20");
    os_GetStringInput("Number of periods: ", buf, sizeof buf);
    long double x = strtold(buf, NULL);
    return x >= 0 ? (int)(x + 0.5) : -1;
}

static long double periodic_rate(long double annual_percent, int periods_per_year) {
    if (periods_per_year <= 0) return 0.0;
    return annual_percent / (100.0 * periods_per_year);
}

/* Nominal annual rate with separate payment and compounding frequencies. */
static long double tvm_rate(long double annual_percent, int payments_per_year,
                            int compounds_per_year) {
    if (payments_per_year <= 0 || compounds_per_year <= 0) return NAN;
    long double base = 1.0L + annual_percent / (100.0L * compounds_per_year);
    if (base <= 0.0L) return NAN;
    return powl(base, (long double)compounds_per_year / payments_per_year) - 1.0L;
}

static long double tvm_factor(long double r, long double n) {
    if (fabsl(r) < EPS) return n;
    return (powl(1.0L + r, n) - 1.0L) / r;
}

static long double tvm_equation(long double r, long double n, long double pv,
                                long double pmt, long double fv) {
    if (r <= -1.0L) return NAN;
    long double growth = powl(1.0L + r, n);
    return pv * growth + pmt * tvm_factor(r, n) + fv;
}

static bool solve_periodic_rate(long double n, long double pv, long double pmt,
                                long double fv, long double *answer) {
    long double last_r = -0.999L;
    long double last_f = tvm_equation(last_r, n, pv, pmt, fv);
    const long double xmin = -6.907755278982137L; /* log(0.001) */
    const long double xmax = 2.397895272798371L;  /* log(11) */
    for (int i = 1; i <= 700; ++i) {
        long double x = xmin + (xmax - xmin) * i / 700.0L;
        long double r = expl(x) - 1.0L;
        long double f = tvm_equation(r, n, pv, pmt, fv);
        if (isfinite(f) && isfinite(last_f) &&
            ((f <= 0.0L && last_f >= 0.0L) || (f >= 0.0L && last_f <= 0.0L))) {
            long double lo = last_r, hi = r, flo = last_f;
            for (int j = 0; j < 90; ++j) {
                long double mid = (lo + hi) / 2.0L;
                long double fm = tvm_equation(mid, n, pv, pmt, fv);
                if ((fm <= 0.0L && flo <= 0.0L) || (fm >= 0.0L && flo >= 0.0L)) {
                    lo = mid; flo = fm;
                } else hi = mid;
            }
            *answer = (lo + hi) / 2.0L;
            return true;
        }
        last_r = r; last_f = f;
    }
    return false;
}

static long double annuity_factor(long double r, int n) {
    if (fabsl(r) < EPS) return (long double)n;
    return (powl(1.0L + r, n) - 1.0L) / r;
}

static long double future_value(long double pv, long double pmt, long double r, int n) {
    long double growth = powl(1.0L + r, n);
    return pv * growth + pmt * annuity_factor(r, n);
}

static long double loan_payment(long double principal, long double r, int n) {
    if (n <= 0) return NAN;
    if (fabsl(r) < EPS) return principal / n;
    return principal * r / (1.0L - powl(1.0L + r, -n));
}

static void money_line(const char *label, long double value) {
    char out[40];
    snprintf(out, sizeof out, "%s %.2Lf", label, value);
    line(out);
}

static void int_line(const char *label, int value) {
    char out[40];
    snprintf(out, sizeof out, "%s %d", label, value);
    line(out);
}

static void signed_money_line(const char *label, long double value) {
    money_line(label, value);
    line(value < 0.0L ? "Negative = money paid" : "Positive = money received");
}

static void savings_value(void) {
    long double pv = number("BALANCE | STEP 1 OF 5", "Starting balance: ");
    long double pmt = number("BALANCE | STEP 2 OF 5", "Regular deposit (or 0): ");
    long double apr = number("BALANCE | STEP 3 OF 5", "Annual interest %: ");
    int ppy = frequency("BALANCE | STEP 4 OF 5");
    int n = period_count("BALANCE | STEP 5 OF 5");
    if (ppy <= 0 || n < 0 || 1.0 + periodic_rate(apr, ppy) <= 0) {
        heading("ERROR"); line("Check periods and rate."); wait_key(); return;
    }
    heading("SAVINGS RESULT");
    money_line("Future value:", future_value(pv, pmt, periodic_rate(apr, ppy), n));
    int_line("Deposits made:", n);
    line("Deposits assumed at end.");
    wait_key();
}

static void savings_target(void) {
    long double pv = number("TARGET | STEP 1 OF 5", "Starting balance: ");
    long double pmt = number("TARGET | STEP 2 OF 5", "Regular deposit (or 0): ");
    long double apr = number("TARGET | STEP 3 OF 5", "Annual interest %: ");
    int ppy = frequency("TARGET | STEP 4 OF 5");
    long double target = number("TARGET | STEP 5 OF 5", "Amount to exceed: ");
    long double r = periodic_rate(apr, ppy), balance = pv;
    int n = 0;
    if (ppy <= 0 || 1.0 + r <= 0 || (pmt <= 0 && r <= 0 && pv <= target)) {
        heading("ERROR"); line("Target cannot be reached."); wait_key(); return;
    }
    while (balance <= target && n < 100000) {
        balance = balance * (1.0 + r) + pmt;
        ++n;
    }
    heading("TARGET RESULT");
    int_line("Complete periods:", n);
    money_line("Balance then:", balance);
    money_line("Previous balance:", n ? (balance - pmt) / (1.0 + r) : pv);
    wait_key();
}

static void standard_loan(void) {
    long double price = number("LOAN | STEP 1 OF 5", "Purchase price: ");
    long double down_pct = number("LOAN | STEP 2 OF 5", "Down payment %: ");
    long double apr = number("LOAN | STEP 3 OF 5", "Annual interest %: ");
    int ppy = frequency("LOAN | STEP 4 OF 5");
    long double years = number("LOAN | STEP 5 OF 5", "Length in years: ");
    int n = (int)floorl(years * ppy + 0.5L);
    long double principal = price * (1.0L - down_pct / 100.0L);
    long double payment = loan_payment(principal, periodic_rate(apr, ppy), n);
    if (price < 0 || principal < 0 || ppy <= 0 || n <= 0 || !isfinite(payment)) {
        heading("ERROR"); line("Check the entered values."); wait_key(); return;
    }
    heading("LOAN RESULT");
    money_line("Down payment:", price - principal);
    money_line("Loan principal:", principal);
    money_line("Payment:", payment);
    money_line("Total paid:", payment * n);
    money_line("Total interest:", payment * n - principal);
    int_line("Payments:", n);
    wait_key();
}

static void faster_loan(void) {
    long double principal = number("EXTRA PAY | STEP 1 OF 5", "Original loan amount: ");
    long double apr = number("EXTRA PAY | STEP 2 OF 5", "Annual interest %: ");
    int ppy = frequency("EXTRA PAY | STEP 3 OF 5");
    long double years = number("EXTRA PAY | STEP 4 OF 5", "Original years: ");
    long double payment = number("EXTRA PAY | STEP 5 OF 5", "New larger payment: ");
    long double r = periodic_rate(apr, ppy), balance = principal, total = 0.0L;
    int original_n = (int)floorl(years * ppy + 0.5L);
    int full = 0;
    if (principal <= 0 || ppy <= 0 || original_n <= 0 || payment <= principal * r) {
        heading("ERROR"); line("Payment is too small."); wait_key(); return;
    }
    while (balance * (1.0 + r) > payment && full < 100000) {
        balance = balance * (1.0 + r) - payment;
        total += payment;
        ++full;
    }
    long double final_payment = balance * (1.0L + r);
    total += final_payment;
    heading("PAYOFF RESULT");
    int_line("Full payments:", full);
    int_line("Total payments:", full + (final_payment > EPS));
    money_line("Final payment:", final_payment);
    money_line("Total paid:", total);
    money_line("Interest paid:", total - principal);
    money_line("Amount saved:", loan_payment(principal, r, original_n) * original_n - total);
    wait_key();
}

static void depreciation(void) {
    long double initial = number("VALUE LOSS | STEP 1 OF 3", "Starting value: ");
    long double final = number("VALUE LOSS | STEP 2 OF 3", "Value at the end: ");
    long double years = number("VALUE LOSS | STEP 3 OF 3", "Number of years: ");
    if (initial <= 0 || final < 0 || final > initial || years <= 0) {
        heading("ERROR"); line("Need 0 <= end <= start."); wait_key(); return;
    }
    long double rate = final == 0 ? 100.0L : 100.0L * (1.0L - powl(final / initial, 1.0L / years));
    heading("DEPRECIATION RESULT");
    money_line("Annual rate %:", rate);
    line("Constant reducing rate.");
    wait_key();
}

static int choose(const char *title, const char *const items[], int count) {
    int selected = 0;
    for (;;) {
        heading(title);
        for (int i = 0; i < count; ++i) {
            os_PutStrFull(i == selected ? ">" : " ");
            line(items[i]);
        }
        line(""); line("ENTER: choose  CLEAR: back");
        uint8_t key;
        while (!(key = os_GetCSC())) {}
        if (key == sk_Clear || key == sk_Mode) return -1;
        if (key == sk_Up) selected = (selected + count - 1) % count;
        if (key == sk_Down) selected = (selected + 1) % count;
        if (key == sk_Enter) return selected;
        while (os_GetCSC()) {}
    }
}

enum tvm_mode { MODE_SAVINGS, MODE_LOAN, MODE_ANNUITY };

static void sign_guide(enum tvm_mode mode) {
    heading("TVM SIGN GUIDE");
    if (mode == MODE_SAVINGS) {
        line("SAVINGS ACCOUNT");
        line("PV: negative (deposit)");
        line("PMT: 0 or negative");
        line("FV: positive (receive)");
    } else if (mode == MODE_LOAN) {
        line("LOAN");
        line("PV: positive (borrowed)");
        line("PMT: negative (repaid)");
        line("FV: 0 or negative");
    } else {
        line("RETIREMENT ANNUITY");
        line("PV: negative (fund)");
        line("PMT: positive withdrawal");
        line("FV: positive or zero");
    }
    line("Money out = negative");
    line("Money in = positive");
    wait_key();
}

static void tvm_solver(enum tvm_mode mode) {
    static const char *const vars[] = {
        "N: number of payments",
        "I%: annual interest",
        "PV: starting amount",
        "PMT: regular payment",
        "FV: ending amount"
    };
    sign_guide(mode);
    int missing = choose("WHICH VALUE IS MISSING?", vars, 5);
    if (missing < 0) return;

    long double n = 0.0L, annual = 0.0L, pv = 0.0L, pmt = 0.0L, fv = 0.0L;
    if (missing != 0) n = number("TVM | TOTAL PERIODS", "N: ");
    if (missing != 1) annual = number("TVM | ANNUAL RATE", "I%: ");
    if (missing != 2) pv = number("TVM | SIGNED VALUE", "PV: ");
    if (missing != 3) pmt = number("TVM | SIGNED VALUE", "PMT: ");
    if (missing != 4) fv = number("TVM | SIGNED VALUE", "FV: ");
    int py = frequency("P/Y | PAYMENT FREQUENCY");
    int cy = frequency("C/Y | COMPOUNDING");
    if (py <= 0 || cy <= 0 || (missing != 0 && n < 0.0L)) {
        heading("TVM ERROR"); line("Check N, P/Y, and C/Y."); wait_key(); return;
    }

    long double result = NAN;
    bool ok = true;
    if (missing == 1) {
        long double r;
        ok = n > 0.0L && solve_periodic_rate(n, pv, pmt, fv, &r);
        if (ok) result = 100.0L * cy *
            (powl(1.0L + r, (long double)py / cy) - 1.0L);
    } else {
        long double r = tvm_rate(annual, py, cy);
        ok = isfinite(r) && r > -1.0L;
        if (ok && missing == 0) {
            if (fabsl(r) < EPS) {
                ok = fabsl(pmt) > EPS;
                if (ok) result = -(fv + pv) / pmt;
            } else {
                long double denominator = pv + pmt / r;
                long double ratio = (pmt / r - fv) / denominator;
                ok = fabsl(denominator) > EPS && ratio > 0.0L;
                if (ok) result = logl(ratio) / logl(1.0L + r);
            }
        } else if (ok) {
            long double growth = powl(1.0L + r, n);
            long double factor = tvm_factor(r, n);
            if (missing == 2) {
                ok = fabsl(growth) > EPS;
                if (ok) result = -(fv + pmt * factor) / growth;
            } else if (missing == 3) {
                ok = fabsl(factor) > EPS;
                if (ok) result = -(fv + pv * growth) / factor;
            } else result = -(pv * growth + pmt * factor);
        }
    }

    if (!ok || !isfinite(result) || (missing == 0 && result < 0.0L)) {
        heading("NO TVM SOLUTION");
        line("Check signs and entries.");
        line("Cash flows need opposite");
        line("signs to balance.");
        wait_key(); return;
    }
    heading("TVM RESULT");
    if (missing == 0) money_line("N periods:", result);
    else if (missing == 1) money_line("Annual I%:", result);
    else if (missing == 2) signed_money_line("PV:", result);
    else if (missing == 3) signed_money_line("PMT:", result);
    else signed_money_line("FV:", result);
    line("Payments occur at end.");
    wait_key();
}

static void outstanding_balance(void) {
    long double principal = number("BALANCE | STEP 1 OF 6", "Original principal: ");
    long double annual = number("BALANCE | STEP 2 OF 6", "Annual interest %: ");
    int py = frequency("BALANCE | STEP 3 OF 6");
    int cy = frequency("BALANCE | STEP 4 OF 6");
    long double payment = number("BALANCE | STEP 5 OF 6", "Payment amount (+): ");
    int made = period_count("BALANCE | STEP 6 OF 6");
    long double r = tvm_rate(annual, py, cy);
    if (principal < 0.0L || payment < 0.0L || made < 0 || !isfinite(r)) {
        heading("ERROR"); line("Check the entered values."); wait_key(); return;
    }
    long double balance = principal * powl(1.0L + r, made) - payment * tvm_factor(r, made);
    heading("LOAN BALANCE RESULT");
    money_line("Outstanding:", balance > 0.0L ? balance : 0.0L);
    int_line("Payments made:", made);
    wait_key();
}

static void loan_total_interest(void) {
    long double principal = number("INTEREST | STEP 1 OF 4", "Principal borrowed: ");
    long double payment = number("INTEREST | STEP 2 OF 4", "Payment amount (+): ");
    int n = period_count("INTEREST | STEP 3 OF 4");
    long double outstanding = number("INTEREST | STEP 4 OF 4", "Balance left (or 0): ");
    long double total = payment * n;
    heading("TOTAL INTEREST RESULT");
    money_line("Payments made:", total);
    money_line("Balance still owed:", outstanding);
    money_line("Total interest:", total + outstanding - principal);
    wait_key();
}

static void annuity_withdrawal(void) {
    long double fund = number("WITHDRAW | STEP 1 OF 6", "Retirement fund (+): ");
    long double ending = number("WITHDRAW | STEP 2 OF 6", "Desired ending balance: ");
    long double annual = number("WITHDRAW | STEP 3 OF 6", "Annual interest %: ");
    int py = frequency("WITHDRAW | STEP 4 OF 6");
    int cy = frequency("WITHDRAW | STEP 5 OF 6");
    int n = period_count("WITHDRAW | STEP 6 OF 6");
    long double r = tvm_rate(annual, py, cy), factor = tvm_factor(r, n);
    if (fund < 0.0L || ending < 0.0L || n <= 0 || !isfinite(r) || fabsl(factor) < EPS) {
        heading("ERROR"); line("Check the entered values."); wait_key(); return;
    }
    long double withdrawal = -((long double)ending + (-fund) * powl(1.0L + r, n)) / factor;
    heading("ANNUITY RESULT");
    money_line("Withdrawal each time:", withdrawal);
    int_line("Withdrawals:", n);
    line("Withdrawals are at end.");
    wait_key();
}

static void annuity_balance(void) {
    long double fund = number("REMAIN | STEP 1 OF 6", "Retirement fund (+): ");
    long double withdrawal = number("REMAIN | STEP 2 OF 6", "Withdrawal amount (+): ");
    long double annual = number("REMAIN | STEP 3 OF 6", "Annual interest %: ");
    int py = frequency("REMAIN | STEP 4 OF 6");
    int cy = frequency("REMAIN | STEP 5 OF 6");
    int n = period_count("REMAIN | STEP 6 OF 6");
    long double r = tvm_rate(annual, py, cy);
    if (fund < 0.0L || withdrawal < 0.0L || n < 0 || !isfinite(r)) {
        heading("ERROR"); line("Check the entered values."); wait_key(); return;
    }
    long double remaining = fund * powl(1.0L + r, n) - withdrawal * tvm_factor(r, n);
    heading("ANNUITY RESULT");
    money_line("Remaining balance:", remaining);
    int_line("Withdrawals made:", n);
    wait_key();
}

static void vocabulary(void) {
    heading("VOCABULARY 1 OF 3");
    line("INFLATION: prices rise;");
    line("buying power falls.");
    line("DEPRECIATION: an asset");
    line("loses value over time.");
    line("PRINCIPAL: original sum");
    line("borrowed or invested.");
    wait_key();
    heading("VOCABULARY 2 OF 3");
    line("INTEREST: cost of a loan");
    line("or earnings on savings.");
    line("OUTSTANDING BALANCE:");
    line("amount still owed.");
    line("LOAN: borrowed money");
    line("repaid with interest.");
    wait_key();
    heading("VOCABULARY 3 OF 3");
    line("ANNUITY: equal payments");
    line("made at regular intervals.");
    line("PV: value at the start.");
    line("PMT: regular cash flow.");
    line("FV: value at the end.");
    line("N: number of payments.");
    wait_key();
}

static void recommendation(const char *tool, const char *reason) {
    heading("RECOMMENDED SOLVER");
    line(tool);
    line("");
    line("Reason:");
    line(reason);
    line("");
    line("Rule confidence: HIGH");
    line("Press a key to launch.");
    while (os_GetCSC()) {}
    while (!os_GetCSC()) {}
    while (os_GetCSC()) {}
}

static void decision_assistant(void) {
    static const char *const kinds[] = {
        "Saving / investing",
        "Borrowed money / loan",
        "Retirement withdrawals",
        "Item losing value"
    };
    static const char *const saving_goals[] = {
        "Amount after time",
        "Time to reach a goal",
        "A TVM letter is missing"
    };
    static const char *const loan_goals[] = {
        "Payment from price/down",
        "Balance still owed",
        "Total interest paid",
        "Paying extra / early",
        "A TVM letter is missing"
    };
    static const char *const annuity_goals[] = {
        "Withdrawal amount",
        "Balance remaining",
        "A TVM letter is missing"
    };

    int kind = choose("DECISION ASSISTANT", kinds, 4);
    if (kind < 0) return;
    if (kind == 0) {
        int goal = choose("WHAT DOES IT ASK?", saving_goals, 3);
        if (goal == 0) {
            recommendation("Savings > Ending balance", "It asks for money later.");
            savings_value();
        } else if (goal == 1) {
            recommendation("Savings > Time to target", "It asks how long.");
            savings_target();
        } else if (goal == 2) {
            recommendation("Savings > Solve missing", "One TVM value is unknown.");
            tvm_solver(MODE_SAVINGS);
        }
    } else if (kind == 1) {
        int goal = choose("WHAT DOES IT ASK?", loan_goals, 5);
        if (goal == 0) {
            recommendation("Loan > Car price/down", "Price and deposit are given.");
            standard_loan();
        } else if (goal == 1) {
            recommendation("Loan > Outstanding", "It asks what is still owed.");
            outstanding_balance();
        } else if (goal == 2) {
            recommendation("Loan > Total interest", "It asks interest paid.");
            loan_total_interest();
        } else if (goal == 3) {
            recommendation("Loan > Paying extra", "A larger payment is used.");
            faster_loan();
        } else if (goal == 4) {
            recommendation("Loan > Solve missing", "One TVM value is unknown.");
            tvm_solver(MODE_LOAN);
        }
    } else if (kind == 2) {
        int goal = choose("WHAT DOES IT ASK?", annuity_goals, 3);
        if (goal == 0) {
            recommendation("Annuity > Withdrawal", "It asks regular income.");
            annuity_withdrawal();
        } else if (goal == 1) {
            recommendation("Annuity > Remaining", "It asks money left later.");
            annuity_balance();
        } else if (goal == 2) {
            recommendation("Annuity > Solve missing", "One TVM value is unknown.");
            tvm_solver(MODE_ANNUITY);
        }
    } else {
        recommendation("Depreciation", "The item loses value.");
        depreciation();
    }
}

static void help_chooser(void) {
    heading("CHEAT GUIDE 1 OF 5");
    line("SAVINGS QUESTIONS");
    line("Amount after some time:");
    line(" Savings > Ending balance");
    line("When will goal be reached:");
    line(" Savings > Time to target");
    line("Any TVM letter missing:");
    line(" Savings > Solve missing");
    wait_key();
    heading("CHEAT GUIDE 2 OF 5");
    line("LOAN QUESTIONS");
    line("Car price + down payment:");
    line(" Loans > Car price/down");
    line("Balance after payments:");
    line(" Loans > Outstanding");
    line("Interest paid:");
    line(" Loans > Total interest");
    wait_key();
    heading("CHEAT GUIDE 3 OF 5");
    line("MORE LOAN QUESTIONS");
    line("Higher payment / early:");
    line(" Loans > Paying extra");
    line("Missing N, I, PV, PMT, FV:");
    line(" Loans > Solve missing");
    line("Loan signs: PV +, PMT -");
    line("FV is 0 or negative.");
    wait_key();
    heading("CHEAT GUIDE 4 OF 5");
    line("RETIREMENT ANNUITY");
    line("How much can I withdraw:");
    line(" Annuity > Withdrawal");
    line("Money left after draws:");
    line(" Annuity > Remaining");
    line("Any TVM letter missing:");
    line(" Annuity > Solve missing");
    wait_key();
    heading("CHEAT GUIDE 5 OF 5");
    line("N = number of payments");
    line("I% = annual interest rate");
    line("PV = value at the start");
    line("PMT = regular payment");
    line("FV = value at the end");
    line("No regular payment: PMT=0");
    line("Value falls: Depreciation");
    wait_key();
}

int main(void) {
    static const char *const main_items[] = {
        "Savings accounts",
        "Loans",
        "Retirement annuities",
        "Depreciation",
        "Vocabulary / help"
    };
    static const char *const save_items[] = {
        "Solve any missing value",
        "Find ending balance",
        "Find time to target"
    };
    static const char *const loan_items[] = {
        "Solve any missing value",
        "Car price / down payment",
        "Outstanding balance",
        "Total interest",
        "Paying extra / payoff"
    };
    static const char *const annuity_items[] = {
        "Solve any missing value",
        "Find withdrawal amount",
        "Find remaining balance"
    };
    static const char *const help_items[] = {
        "Solver decision assistant",
        "Solver cheat guide",
        "Vocabulary definitions"
    };
    for (;;) {
        int category = choose("CHOOSE PROBLEM TYPE", main_items, 5);
        if (category < 0) break;
        if (category == 0) {
            int choice = choose("SAVINGS ACCOUNT", save_items, 3);
            if (choice == 0) tvm_solver(MODE_SAVINGS);
            else if (choice == 1) savings_value();
            else if (choice == 2) savings_target();
        } else if (category == 1) {
            int choice = choose("LOAN TOOLS", loan_items, 5);
            if (choice == 0) tvm_solver(MODE_LOAN);
            else if (choice == 1) standard_loan();
            else if (choice == 2) outstanding_balance();
            else if (choice == 3) loan_total_interest();
            else if (choice == 4) faster_loan();
        } else if (category == 2) {
            int choice = choose("RETIREMENT ANNUITY", annuity_items, 3);
            if (choice == 0) tvm_solver(MODE_ANNUITY);
            else if (choice == 1) annuity_withdrawal();
            else if (choice == 2) annuity_balance();
        } else if (category == 3) depreciation();
        else {
            int choice = choose("HELP", help_items, 3);
            if (choice == 0) decision_assistant();
            else if (choice == 1) help_chooser();
            else if (choice == 2) vocabulary();
        }
    }
    os_ClrHome();
    return 0;
}
