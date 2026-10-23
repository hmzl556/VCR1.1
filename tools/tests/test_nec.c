#include "mock_hw.h"
#include "ir_nec.h"
int main(void){unsigned u,c;uint16_t w[67];uint8_t a,b;
 for(u=0;u<256;u++)for(c=0;c<256;c++){
  unsigned i;uint32_t bits=u|((uint32_t)(uint8_t)~u<<8)|(c<<16)|((uint32_t)(uint8_t)~c<<24);
  w[0]=9000;w[1]=4500;w[66]=600;for(i=0;i<32;i++){w[2+2*i]=560;w[3+2*i]=(bits&(1UL<<i))?1680:560;}
  assert(ir_nec_decode(w,67,&a,&b)&&a==u&&b==c);
 }
 w[2]=0;assert(!ir_nec_decode(w,67,&a,&b));w[2]=560;
 w[3]=1100;assert(!ir_nec_decode(w,67,&a,&b));w[3]=1680;
 w[66]=0;assert(!ir_nec_decode(w,67,&a,&b));assert(!ir_nec_decode(w,66,&a,&b));
puts("NEC decoder: exhaustive 65536 pairs and malformed timing PASS");return 0;}
