/****************************************************************//**
 *
 * @file hw.h
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    Hardware Abstraction
 *
 * Copyright (c) Deltatee Enterprises Ltd. 2013
 * All rights reserved.
 *
 ********************************************************************/

/* 
 * Redistribution and use in source and binary forms, with or without
 * modification,are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Logan Gunthorpe <logang@deltatee.com>
 *
 */


#include <devices/stm32f2xx.h>

#include <stmlib/clock.h>
#include <stmlib/gpio.h>

#define DBG_USART        USART3
#define DBG_DMA_STREAM   DMA1_Stream3
#define DBG_DMA_IRQ      DMA1_Stream3_IRQn
#define DBG_DMA_CH       DMA_SxCR_CHSEL_CH4

#define TICK_TIMER       SysTick
#define TICK_TIMER_IRQ   SysTick_IRQn

#define ETH_RMII_MDINT_IRQ  EXTI3_IRQn

static inline void _clock_setup(void)
{
    clock_pll_setup(CLOCK_PLL_SRC_HSI, 0, 120000000);
    clock_flash_cfg(VRANGE_2V7_TO_3V6, clock_pll_freq);
    clock_select(CLOCK_SELECT_PLL, 0, CLOCK_AHB_DIV_NONE,
                 CLOCK_APB_DIV_NONE, CLOCK_APB_DIV_NONE);
}

static inline void hw_init(void)
{
    _clock_setup();

    //Clock Enables
    RCC->AHB1ENR = (RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN |
                    RCC_AHB1ENR_GPIOFEN |
                    RCC_AHB1ENR_GPIOGEN |
                    RCC_AHB1ENR_DMA1EN  |
                    RCC_AHB1ENR_ETHMACRXEN |
                    RCC_AHB1ENR_ETHMACTXEN |
                    RCC_AHB1ENR_ETHMACEN);
    RCC->AHB2ENR = RCC_AHB2ENR_RNGEN;
    RCC->APB1ENR = RCC_APB1ENR_USART3EN;
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;

    SYSCFG->PMC = SYSCFG_PMC_MII_RMII_SEL;

    //Port A
    GPIOA->MODER = (GPIO_MODER_MODER1_AF | //ETH_RMII_REF_CLK
                    GPIO_MODER_MODER2_AF | //ETH_RMII_MDIO
                    GPIO_MODER_MODER3_AF | //ETH_RMII_MDINT
                    GPIO_MODER_MODER7_AF); //ETH_RMII_RCS_DV

    GPIOA->OSPEEDR = (GPIO_OSPEEDR_OSPEEDR1_FAST |
                      GPIO_OSPEEDR_OSPEEDR2_FAST |
                      GPIO_OSPEEDR_OSPEEDR3_FAST |
                      GPIO_OSPEEDR_OSPEEDR7_FAST);

    gpio_af_select(GPIOA, 1, GPIO_AF_ETH);
    gpio_af_select(GPIOA, 2, GPIO_AF_ETH);
    gpio_af_select(GPIOA, 3, GPIO_AF_ETH);
    gpio_af_select(GPIOA, 7, GPIO_AF_ETH);

    SYSCFG->EXTICR[0] = SYSCFG_EXTICR1_EXTI3_PA;
    EXTI->IMR = EXTI_IMR_MR3;
    EXTI->FTSR = EXTI_FTSR_TR3;

    //Port B
    GPIOB->MODER = GPIO_MODER_MODER11_AF; //ETH_RMII_TX_EN
    GPIOC->OSPEEDR = GPIO_OSPEEDR_OSPEEDR11_FAST;
    gpio_af_select(GPIOB, 11, GPIO_AF_ETH);

    //Port C
    GPIOC->MODER = (GPIO_MODER_MODER1_AF  |   //ETH_RMII_MDC
                    GPIO_MODER_MODER4_AF  |   //ETH_RMII_RXD0
                    GPIO_MODER_MODER5_AF  |   //ETH_RMII_RXD1
                    GPIO_MODER_MODER10_AF |   //DEBUG_TX
                    GPIO_MODER_MODER11_AF);   //DEBUG_RX
    GPIOC->OSPEEDR = (GPIO_OSPEEDR_OSPEEDR1_FAST |
                      GPIO_OSPEEDR_OSPEEDR4_FAST |
                      GPIO_OSPEEDR_OSPEEDR5_FAST);
    GPIOC->PUPDR = GPIO_PUPDR_PUPDR10_PU;

    gpio_af_select(GPIOC, 1,  GPIO_AF_ETH);
    gpio_af_select(GPIOC, 4,  GPIO_AF_ETH);
    gpio_af_select(GPIOC, 5,  GPIO_AF_ETH);
    gpio_af_select(GPIOC, 10, GPIO_AF_USART3);
    gpio_af_select(GPIOC, 11, GPIO_AF_USART3);

    //Port F
    GPIOF->MODER = (GPIO_MODER_MODER6_OUT | //STAT1
                    GPIO_MODER_MODER7_OUT | //STAT2
                    GPIO_MODER_MODER8_OUT | //STAT3
                    GPIO_MODER_MODER9_OUT); //STAT4

    //Port G
    GPIOG->MODER = (GPIO_MODER_MODER13_AF | //ETH_RMII_TXD0
                    GPIO_MODER_MODER14_AF); //ETH_RMII_TXD1

    GPIOG->OSPEEDR = (GPIO_OSPEEDR_OSPEEDR13_FAST |
                      GPIO_OSPEEDR_OSPEEDR14_FAST);

    gpio_af_select(GPIOG, 13, GPIO_AF_ETH);
    gpio_af_select(GPIOG, 14, GPIO_AF_ETH);

}

enum led {
    LED_STAT1 = 1 << 6,
    LED_STAT2 = 1 << 7,
    LED_STAT3 = 1 << 8,
    LED_STAT4 = 1 << 9,
};

static inline void hw_set_led(enum led led)
{
    GPIOF->BSRRL = led;
}

static inline void hw_clr_led(enum led led)
{
    GPIOF->BSRRH = led;
}

static inline void hw_eth_rmii_mdint_irq_clear(void)
{
    EXTI->PR = 1 << 3;
}
