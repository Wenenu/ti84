# TVM Easy for TI-84 Plus CE

TVM Easy is a guided native program for the non-Python TI-84 Plus CE. It includes:

- a full-color graphical interface with highlighted cards, guided step screens, large numeric entry fields, and persistent key hints;

- a general TVM solver for any one missing `N`, `I%`, `PV`, `PMT`, or `FV`, with separate `P/Y` and `C/Y`;
- future value with end-of-period deposits;
- the first complete period when savings exceed a target;
- loan principal, regular payment, total payments, total interest, and outstanding balance;
- accelerated payoff count, smaller final payment, total paid, and savings versus the original loan;
- retirement-annuity withdrawals and remaining balances;
- constant reducing-balance depreciation rate.

The general solver displays the required sign convention before entry:

- Savings: `PV` negative, `PMT` zero or negative, `FV` positive.
- Loans: `PV` positive, `PMT` negative, `FV` zero or negative.
- Retirement annuities: `PV` negative, `PMT` positive, `FV` positive or zero.

All TVM payments occur at the end of a period. The Vocabulary section defines inflation, depreciation, principal, interest, outstanding balance, loan, annuity, `PV`, `PMT`, `FV`, and `N`.

Rates are entered as annual percentages. For monthly problems, enter `12` periods/payments per year. All deposits and payments are assumed to occur at the end of each period.

First choose the problem type—Savings, Loan, Retirement Annuity, or Depreciation—then choose the result requested in the question. The built-in five-page **Solver cheat guide** maps common question wording to the correct menu and summarizes the TVM letters and signs:

The offline **Solver decision assistant** asks what kind of money is involved and what the question requests. It displays the matched solver, its reason, and a rule-confidence label, then launches that solver directly. This is a deterministic expert system: it is fast and explainable, but it does not interpret arbitrary natural-language sentences.

- **Savings / investment > Find ending balance** for “calculate the amount after...” questions. Enter `0` for the regular deposit when there are no added deposits.
- **Savings / investment > Find time to target** for “how many complete months until...” questions.
- **Loan / car finance > Find regular payment** when the unknown is the normal monthly payment or total over the original loan term.
- **Loan / car finance > Paying extra / payoff** when a higher payment is given and the question asks for the number of payments, final payment, or amount saved.
- **Depreciation** when an initial value, later value, and elapsed time are given.

## Install and run

Send `bin/TVMEASY.8xp` with TI Connect CE. On OS 5.8, first start arTIfiCE through the Cabri Jr. exploit, select `TVMEASY`, and press Enter. Use Up/Down and Enter in the menu. Numeric screens accept the number keys, decimal point, `(-)` for a negative sign, Del to erase, and Enter to continue. Clear or Mode goes back from a menu.

To keep the program through a normal RAM reset, archive it from `2nd` > `MEM` > `Mem Management/Delete` > `Prgm`, highlight `TVMEASY`, and press Enter. A full `Reset All` still deletes third-party programs.

## Example mappings

- Mina question (a): Savings > Find ending balance; 1000 start, 100 deposit, 4%, 12/year, 24 periods (two years of monthly periods).
- Mina question (b): Savings > Find time to target; same entries, target 5000.
- Ruben payment: Loan > Find regular payment; 285000 price, 25% down, 4.5%, 12/year, 5 years.
- Ruben at 4600/month: Loan > Paying extra / payoff; use the displayed loan principal, 4.5%, 12/year, 5 original years, and 4600 payment.
- Angel investment: Savings > Find ending balance; 520 start, 0 deposit, 1.2%, 4/year, 20 periods.
- Angel phone: Depreciation; 520 initial, 30 final, 5 years.

The source is in `src/main.c` and builds with the CE C toolchain.
