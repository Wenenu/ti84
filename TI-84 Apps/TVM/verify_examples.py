"""Independent checks for the worksheet examples shown with TVM Easy."""
import math


def future_value(pv, payment, annual_percent, periods_per_year, periods):
    rate = annual_percent / (100 * periods_per_year)
    factor = periods if rate == 0 else ((1 + rate) ** periods - 1) / rate
    return pv * (1 + rate) ** periods + payment * factor


def periods_to_exceed(pv, payment, annual_percent, periods_per_year, target):
    rate = annual_percent / (100 * periods_per_year)
    periods = 0
    while pv <= target:
        pv = pv * (1 + rate) + payment
        periods += 1
    return periods, pv


def regular_loan(price, down_percent, annual_percent, payments_per_year, years):
    principal = price * (1 - down_percent / 100)
    count = round(payments_per_year * years)
    rate = annual_percent / (100 * payments_per_year)
    payment = principal * rate / (1 - (1 + rate) ** -count)
    return principal, payment, payment * count


def faster_loan(principal, annual_percent, payments_per_year, years, payment):
    rate = annual_percent / (100 * payments_per_year)
    balance = principal
    full = 0
    while balance * (1 + rate) > payment:
        balance = balance * (1 + rate) - payment
        full += 1
    final = balance * (1 + rate)
    fast_total = full * payment + final
    count = round(years * payments_per_year)
    normal = principal * rate / (1 - (1 + rate) ** -count)
    return full + 1, final, fast_total, normal * count - fast_total


def close(actual, expected, tolerance=0.011):
    assert abs(actual - expected) <= tolerance, (actual, expected)


close(future_value(1000, 100, 4, 12, 24), 3577.43)
assert periods_to_exceed(1000, 100, 4, 12, 5000)[0] == 37

principal, payment, total = regular_loan(285000, 25, 4.5, 12, 5)
close(principal, 213750)
close(payment, 3984.95)
assert round(total) == 239097

count, final, fast_total, saved = faster_loan(213750, 4.5, 12, 5, 4600)
assert count == 52
close(final, 706.80)
close(fast_total, 235306.80)
assert round(saved) == 3790

close(future_value(520, 0, 1.2, 4, 20), 552.11)
depreciation = 100 * (1 - (30 / 520) ** (1 / 5))
close(depreciation, 43.48)

print("All worksheet checks passed.")
print("Mina: 3577.43 and 37 months")
print("Ruben: 3984.95/month; 239097 total; 52 payments; 706.80 final; 3790 saved")
print("Angel: 552.11 balance and 43.48% depreciation")

# General TVM sign-convention checks: PV*g + PMT*a + FV = 0.
def tvm_rate(annual_percent, payments_per_year, compounds_per_year):
    return (1 + annual_percent / (100 * compounds_per_year)) ** (
        compounds_per_year / payments_per_year
    ) - 1


def tvm_factor(rate, periods):
    return periods if rate == 0 else ((1 + rate) ** periods - 1) / rate


rate = tvm_rate(4.5, 12, 12)
growth = (1 + rate) ** 60
factor = tvm_factor(rate, 60)
pv = 213750
pmt = -(0 + pv * growth) / factor
fv = -(pv * growth + pmt * factor)
close(pmt, -3984.95)
close(fv, 0)
recovered_pv = -(fv + pmt * factor) / growth
close(recovered_pv, pv)

# Outstanding loan balance after 12 of the 60 Ruben payments.
outstanding = pv * (1 + rate) ** 12 - (-pmt) * tvm_factor(rate, 12)
close(outstanding, 174751.59)

# Retirement annuity: withdraw monthly for 20 years and finish at zero.
retire_rate = tvm_rate(5, 12, 12)
retire_factor = tvm_factor(retire_rate, 240)
withdrawal = 500000 * (1 + retire_rate) ** 240 / retire_factor
close(withdrawal, 3299.78)
remaining_after_60 = (
    500000 * (1 + retire_rate) ** 60
    - withdrawal * tvm_factor(retire_rate, 60)
)
close(remaining_after_60, 417274.32)

print("General TVM signs, loan balance, and retirement annuity checks passed.")
