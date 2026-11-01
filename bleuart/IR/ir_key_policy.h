#ifndef IR_KEY_POLICY_H
#define IR_KEY_POLICY_H
/* All physical keys except voice; validate range before shifting. */
static unsigned ir_key_learnable(unsigned key)
{ return key>=1u && key<=18u && key!=15u; }
#endif
