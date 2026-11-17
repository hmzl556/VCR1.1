#ifndef IR_KEY_POLICY_H
#define IR_KEY_POLICY_H
/* Only keys explicitly left learnable by the user.
 * 新板键号 (SDK KEY_n, 2026-11-17 枚举):
 *   1=电视机电源, 11=回看, 14=点播, 23=电视机业务 */
static unsigned ir_key_learnable(unsigned key)
{ return key==1u || key==11u || key==14u || key==23u; }
#endif
