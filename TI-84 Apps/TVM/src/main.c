#include <ti/getcsc.h>
#include <graphx.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define EPS 1e-13L

enum {
    C_BG, C_PANEL, C_PANEL_2, C_BLUE, C_CYAN, C_WHITE, C_MUTED,
    C_GREEN, C_RED, C_YELLOW
};

static const uint16_t ui_palette[] = {
    gfx_RGBTo1555(255, 255, 255), /* page background */
    gfx_RGBTo1555(245, 247, 250), /* cards and footer */
    gfx_RGBTo1555(255, 255, 255), /* input fields */
    gfx_RGBTo1555(213, 232, 255), /* selected card/header */
    gfx_RGBTo1555(0, 70, 150),    /* blue accent */
    gfx_RGBTo1555(8, 15, 25),     /* primary text */
    gfx_RGBTo1555(65, 75, 90),    /* secondary text */
    gfx_RGBTo1555(0, 105, 65),
    gfx_RGBTo1555(175, 25, 35),
    gfx_RGBTo1555(130, 80, 0)
};

static int ui_y;

static void ui_text(const char *text, int x, int y, uint8_t color) {
    gfx_SetTextFGColor(color);
    gfx_SetTextTransparentColor(C_BG);
    gfx_PrintStringXY(text, x, y);
}

static void ui_frame(const char *title, bool buffered) {
    if (buffered) gfx_SetDrawBuffer(); else gfx_SetDrawScreen();
    gfx_SetColor(C_BG); gfx_FillScreen(C_BG);
    gfx_SetColor(C_BLUE); gfx_FillRectangle(0, 0, 320, 48);
    gfx_SetColor(C_CYAN); gfx_FillRectangle(0, 46, 320, 2);
    gfx_SetTextScale(1, 1);
    ui_text("TVM EASY", 14, 8, C_CYAN);
    ui_text(title, 14, 27, C_WHITE);
    ui_y = 62;
}

static void ui_footer(const char *left, const char *right) {
    gfx_SetColor(C_PANEL); gfx_FillRectangle(0, 218, 320, 22);
    ui_text(left, 10, 225, C_MUTED);
    if (right) ui_text(right, 320 - gfx_GetStringWidth(right) - 10, 225, C_MUTED);
}

static void heading(const char *title) {
    ui_frame(title, false);
}

static void line(const char *s) {
    if (*s) {
        gfx_SetColor(C_PANEL);
        gfx_FillRectangle(10, ui_y - 5, 300, 19);
        ui_text(s, 18, ui_y, C_WHITE);
    }
    ui_y += 22;
}

static void wait_key(void) {
    ui_footer("ANY KEY  Continue", NULL);
    while (os_GetCSC()) {}
    while (!os_GetCSC()) {}
    while (os_GetCSC()) {}
}

static char digit_for_key(uint8_t key) {
    if (key == sk_0) return '0'; if (key == sk_1) return '1';
    if (key == sk_2) return '2'; if (key == sk_3) return '3';
    if (key == sk_4) return '4'; if (key == sk_5) return '5';
    if (key == sk_6) return '6'; if (key == sk_7) return '7';
    if (key == sk_8) return '8'; if (key == sk_9) return '9';
    return 0;
}

enum input_kind { INPUT_NUMBER, INPUT_FREQUENCY, INPUT_PERIODS };

typedef struct {
    const char *title;
    const char *prompt;
    enum input_kind kind;
    long double value;
    bool set;
} input_field;

static int numeric_input(const char *title, const char *prompt,
                         const char *hint1, const char *hint2,
                         long double initial, bool has_initial,
                         long double *answer) {
    char buf[24] = "";
    size_t len = 0;
    if (has_initial) {
        snprintf(buf, sizeof buf, "%.12Lg", initial);
        len = strlen(buf);
    }
    for (;;) {
        ui_frame(title, true);
        ui_text(prompt, 16, 72, C_MUTED);
        gfx_SetColor(C_PANEL_2); gfx_FillRectangle(14, 92, 292, 48);
        gfx_SetColor(C_CYAN); gfx_Rectangle(14, 92, 292, 48);
        gfx_SetTextScale(2, 2);
        ui_text(len ? buf : "0", 25, 106, len ? C_WHITE : C_MUTED);
        gfx_SetTextScale(1, 1);
        if (hint1) ui_text(hint1, 17, 158, C_CYAN);
        if (hint2) ui_text(hint2, 17, 176, C_MUTED);
        ui_footer("LEFT  Previous", "ENTER  Next");
        gfx_SwapDraw();

        uint8_t key;
        while (!(key = os_GetCSC())) {}
        char digit = digit_for_key(key);
        if (digit && len < sizeof buf - 1) {
            buf[len++] = digit; buf[len] = 0;
        } else if (key == sk_DecPnt && len < sizeof buf - 1) {
            bool has_dot = false;
            for (size_t i = 0; i < len; ++i) if (buf[i] == '.') has_dot = true;
            if (!has_dot) { if (!len) buf[len++] = '0'; buf[len++] = '.'; buf[len] = 0; }
        } else if (key == sk_Chs && len < sizeof buf - 1) {
            if (len && buf[0] == '-') { memmove(buf, buf + 1, len); --len; }
            else { memmove(buf + 1, buf, len + 1); buf[0] = '-'; ++len; }
        } else if (key == sk_Del && len) buf[--len] = 0;
        else if (key == sk_Clear) { len = 0; buf[0] = 0; }
        else if (key == sk_Left || key == sk_Mode) {
            while (os_GetCSC()) {}
            return -1;
        }
        else if (key == sk_Enter && len && !(len == 1 && buf[0] == '-')) {
            while (os_GetCSC()) {}
            *answer = strtold(buf, NULL);
            return 1;
        }
        while (os_GetCSC()) {}
    }
}

static bool edit_form(input_field *fields, int count) {
    int index = 0;
    while (index < count) {
        const char *hint1 = "Use (-) for cash paid out";
        const char *hint2 = NULL;
        if (fields[index].kind == INPUT_FREQUENCY) {
            hint1 = "Monthly = 12   Quarterly = 4";
            hint2 = "Annual = 1";
        } else if (fields[index].kind == INPUT_PERIODS) {
            hint1 = "2 years monthly = 24";
            hint2 = "5 years quarterly = 20";
        }
        long double value;
        int action = numeric_input(fields[index].title, fields[index].prompt,
            hint1, hint2, fields[index].value, fields[index].set, &value);
        if (action < 0) {
            if (index == 0) return false;
            --index;
        } else {
            fields[index].value = value;
            fields[index].set = true;
            ++index;
        }
    }
    return true;
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
    input_field f[] = {
        {"BALANCE | STEP 1 OF 5", "Starting balance:", INPUT_NUMBER},
        {"BALANCE | STEP 2 OF 5", "Regular deposit (or 0):", INPUT_NUMBER},
        {"BALANCE | STEP 3 OF 5", "Annual interest %:", INPUT_NUMBER},
        {"BALANCE | STEP 4 OF 5", "Periods per year", INPUT_FREQUENCY},
        {"BALANCE | STEP 5 OF 5", "Total number of periods", INPUT_PERIODS}
    };
    if (!edit_form(f, 5)) return;
    long double pv=f[0].value, pmt=f[1].value, apr=f[2].value;
    int ppy=(int)(f[3].value+.5L), n=(int)(f[4].value+.5L);
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
    input_field f[] = {
        {"TARGET | STEP 1 OF 5", "Starting balance:", INPUT_NUMBER},
        {"TARGET | STEP 2 OF 5", "Regular deposit (or 0):", INPUT_NUMBER},
        {"TARGET | STEP 3 OF 5", "Annual interest %:", INPUT_NUMBER},
        {"TARGET | STEP 4 OF 5", "Periods per year", INPUT_FREQUENCY},
        {"TARGET | STEP 5 OF 5", "Amount to exceed:", INPUT_NUMBER}
    };
    if (!edit_form(f, 5)) return;
    long double pv=f[0].value, pmt=f[1].value, apr=f[2].value, target=f[4].value;
    int ppy=(int)(f[3].value+.5L);
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
    input_field f[] = {
        {"LOAN | STEP 1 OF 5", "Purchase price:", INPUT_NUMBER},
        {"LOAN | STEP 2 OF 5", "Down payment %:", INPUT_NUMBER},
        {"LOAN | STEP 3 OF 5", "Annual interest %:", INPUT_NUMBER},
        {"LOAN | STEP 4 OF 5", "Payments per year", INPUT_FREQUENCY},
        {"LOAN | STEP 5 OF 5", "Length in years:", INPUT_NUMBER}
    };
    if (!edit_form(f, 5)) return;
    long double price=f[0].value, down_pct=f[1].value, apr=f[2].value, years=f[4].value;
    int ppy=(int)(f[3].value+.5L);
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
    input_field f[] = {
        {"EXTRA PAY | STEP 1 OF 5", "Original loan amount:", INPUT_NUMBER},
        {"EXTRA PAY | STEP 2 OF 5", "Annual interest %:", INPUT_NUMBER},
        {"EXTRA PAY | STEP 3 OF 5", "Payments per year", INPUT_FREQUENCY},
        {"EXTRA PAY | STEP 4 OF 5", "Original years:", INPUT_NUMBER},
        {"EXTRA PAY | STEP 5 OF 5", "New larger payment:", INPUT_NUMBER}
    };
    if (!edit_form(f, 5)) return;
    long double principal=f[0].value, apr=f[1].value, years=f[3].value, payment=f[4].value;
    int ppy=(int)(f[2].value+.5L);
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
    input_field f[] = {
        {"VALUE LOSS | STEP 1 OF 3", "Starting value:", INPUT_NUMBER},
        {"VALUE LOSS | STEP 2 OF 3", "Value at the end:", INPUT_NUMBER},
        {"VALUE LOSS | STEP 3 OF 3", "Number of years:", INPUT_NUMBER}
    };
    if (!edit_form(f, 3)) return;
    long double initial=f[0].value, final=f[1].value, years=f[2].value;
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
        const int top = 58;
        const int item_h = count > 4 ? 30 : 39;
        ui_frame(title, true);
        for (int i = 0; i < count; ++i) {
            int y = top + i * item_h;
            bool active = i == selected;
            gfx_SetColor(active ? C_BLUE : C_PANEL);
            gfx_FillRectangle(12, y, 296, item_h - 5);
            if (active) {
                gfx_SetColor(C_CYAN);
                gfx_FillRectangle(12, y, 4, item_h - 5);
            }
            ui_text(items[i], 25, y + (item_h - 13) / 2,
                    active ? C_WHITE : C_MUTED);
            if (active) ui_text(">", 291, y + (item_h - 13) / 2, C_CYAN);
        }
        ui_footer("UP/DOWN  Navigate", "ENTER  Select");
        gfx_SwapDraw();
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
    input_field f[6]; int map[6]; int count = 0;
    static const char *const titles[] = {"TVM | TOTAL PERIODS", "TVM | ANNUAL RATE", "TVM | SIGNED VALUE", "TVM | SIGNED VALUE", "TVM | SIGNED VALUE"};
    static const char *const prompts[] = {"N:", "I%:", "PV:", "PMT:", "FV:"};
    for (int i=0;i<5;++i) if (i!=missing) {
        f[count]=(input_field){titles[i],prompts[i],i==0?INPUT_PERIODS:INPUT_NUMBER,0,false}; map[count++]=i;
    }
    f[count]=(input_field){"P/Y | PAYMENT FREQUENCY","Payments per year",INPUT_FREQUENCY,0,false}; map[count++]=5;
    f[count]=(input_field){"C/Y | COMPOUNDING","Compounds per year",INPUT_FREQUENCY,0,false}; map[count++]=6;
    if (!edit_form(f,count)) return;
    int py=0,cy=0;
    for(int i=0;i<count;++i) {
        if(map[i]==0)n=f[i].value; else if(map[i]==1)annual=f[i].value;
        else if(map[i]==2)pv=f[i].value; else if(map[i]==3)pmt=f[i].value;
        else if(map[i]==4)fv=f[i].value; else if(map[i]==5)py=(int)(f[i].value+.5L);
        else cy=(int)(f[i].value+.5L);
    }
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
    input_field f[] = {
        {"BALANCE | STEP 1 OF 6","Original principal:",INPUT_NUMBER},
        {"BALANCE | STEP 2 OF 6","Annual interest %:",INPUT_NUMBER},
        {"BALANCE | STEP 3 OF 6","Payments per year",INPUT_FREQUENCY},
        {"BALANCE | STEP 4 OF 6","Compounds per year",INPUT_FREQUENCY},
        {"BALANCE | STEP 5 OF 6","Payment amount (+):",INPUT_NUMBER},
        {"BALANCE | STEP 6 OF 6","Payments already made",INPUT_PERIODS}
    };
    if(!edit_form(f,6))return;
    long double principal=f[0].value,annual=f[1].value,payment=f[4].value;
    int py=(int)(f[2].value+.5L),cy=(int)(f[3].value+.5L),made=(int)(f[5].value+.5L);
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
    input_field f[] = {
        {"INTEREST | STEP 1 OF 4","Principal borrowed:",INPUT_NUMBER},
        {"INTEREST | STEP 2 OF 4","Payment amount (+):",INPUT_NUMBER},
        {"INTEREST | STEP 3 OF 4","Number of payments",INPUT_PERIODS},
        {"INTEREST | STEP 4 OF 4","Balance left (or 0):",INPUT_NUMBER}
    };
    if(!edit_form(f,4))return;
    long double principal=f[0].value,payment=f[1].value,outstanding=f[3].value;
    int n=(int)(f[2].value+.5L);
    long double total = payment * n;
    heading("TOTAL INTEREST RESULT");
    money_line("Payments made:", total);
    money_line("Balance still owed:", outstanding);
    money_line("Total interest:", total + outstanding - principal);
    wait_key();
}

static void annuity_withdrawal(void) {
    input_field f[] = {
        {"WITHDRAW | STEP 1 OF 6","Retirement fund (+):",INPUT_NUMBER},
        {"WITHDRAW | STEP 2 OF 6","Desired ending balance:",INPUT_NUMBER},
        {"WITHDRAW | STEP 3 OF 6","Annual interest %:",INPUT_NUMBER},
        {"WITHDRAW | STEP 4 OF 6","Payments per year",INPUT_FREQUENCY},
        {"WITHDRAW | STEP 5 OF 6","Compounds per year",INPUT_FREQUENCY},
        {"WITHDRAW | STEP 6 OF 6","Number of withdrawals",INPUT_PERIODS}
    };
    if(!edit_form(f,6))return;
    long double fund=f[0].value,ending=f[1].value,annual=f[2].value;
    int py=(int)(f[3].value+.5L),cy=(int)(f[4].value+.5L),n=(int)(f[5].value+.5L);
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
    input_field f[] = {
        {"REMAIN | STEP 1 OF 6","Retirement fund (+):",INPUT_NUMBER},
        {"REMAIN | STEP 2 OF 6","Withdrawal amount (+):",INPUT_NUMBER},
        {"REMAIN | STEP 3 OF 6","Annual interest %:",INPUT_NUMBER},
        {"REMAIN | STEP 4 OF 6","Payments per year",INPUT_FREQUENCY},
        {"REMAIN | STEP 5 OF 6","Compounds per year",INPUT_FREQUENCY},
        {"REMAIN | STEP 6 OF 6","Number of withdrawals",INPUT_PERIODS}
    };
    if(!edit_form(f,6))return;
    long double fund=f[0].value,withdrawal=f[1].value,annual=f[2].value;
    int py=(int)(f[3].value+.5L),cy=(int)(f[4].value+.5L),n=(int)(f[5].value+.5L);
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
    gfx_Begin();
    gfx_SetPalette(ui_palette, sizeof ui_palette, 0);
    gfx_SetTextScale(1, 1);
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
    gfx_End();
    return 0;
}
