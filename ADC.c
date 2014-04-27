#include <avr/io.h>
#include <avr/iom8.h>

static unsigned int ADC_value=0;

static void ADC_init()
{
	ADMUX=(1<<REFS0);                         // For Aref=AVcc;
	ADCSRA=(1<<ADEN)|(1 << ADPS2)|(0 << ADPS1)|(1 << ADPS0); 
}

static unsigned int ADC_result(unsigned char ch)
{
   ch=ch&0b00000111;
   ADMUX=(1<<REFS0)|(ch);
   ADCSRA|=(1<<ADSC);
   while((ADCSRA&(1 << ADIF))==0);
		 ADCSRA|=(1<<ADIF);
   ADC_value=(ADCL|ADCH << 8);
   return ADC_value;

}
