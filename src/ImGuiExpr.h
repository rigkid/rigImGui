#pragma once

namespace rigkit {

/**
 * @brief Evaluate a numeric field expression against the current value.
 * @details Leading `+` `*` `/` apply to the current value (`*2`, `+10`, `/2`).
 * `-10` sets a negative; `+-10` subtracts. `x` is the current value (`x*2`).
 * `+ - * / ( )` with usual precedence. False on junk or divide-by-zero.
 * Properties click-to-type calls this; do not hook Dear ImGui for it.
 * @param buf Expression text (already trimmed of leading blanks by the caller).
 * @param current Value in the field before the edit.
 * @param out Result when the function returns true.
 */
bool evalNumericExpr(const char *buf, double current, double &out);

} // namespace rigkit

