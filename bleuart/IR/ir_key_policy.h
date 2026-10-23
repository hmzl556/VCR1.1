#ifndef IR_KEY_POLICY_H
#define IR_KEY_POLICY_H
/* Only keys explicitly left learnable by the user. */
static unsigned ir_key_learnable(unsigned key)
{ return key==1u || key==2u || key==3u || key==17u; }
#endif
