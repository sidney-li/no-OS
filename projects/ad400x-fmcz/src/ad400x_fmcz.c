#include <stdio.h>
#include <sleep.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <xil_cache.h>
#include <xparameters.h>
#include "xil_printf.h"
#include "spi_engine.h"
#include "ad400x.h"
#include "error.h"
#include "delay.h"

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/
#define AD400x_EVB_SAMPLE_NO			1000
#define AD400X_DMA_BASEADDR             XPAR_AXI_AD40XX_DMA_BASEADDR
#define AD400X_SPI_ENGINE_BASEADDR      XPAR_SPI_ADC_AXI_REGMAP_BASEADDR
#define AD400x_SPI_CS                   0

#define SPI_ENGINE_OFFLOAD_EXAMPLE	1

struct spi_engine_init_param spi_eng_init_param  = {
	.type = SPI_ENGINE,
	.spi_engine_baseaddr = AD400X_SPI_ENGINE_BASEADDR,
	.cs_delay = 2,
	.data_width = 32,
};

struct ad400x_init_param ad400x_init_param = {
	.spi_init = {
		.chip_select = AD400x_SPI_CS,
		.max_speed_hz = 200000,
		.mode = SPI_MODE_0,
		.platform_ops = &spi_eng_platform_ops,
		.extra = (void*)&spi_eng_init_param,
	},
	ID_AD4003, 20, /* dev_id, num_bits */
	1,0,0,0,
};

int main()
{
	struct ad400x_dev *dev;
	uint32_t *offload_data;
	uint32_t adc_data;
	struct spi_engine_offload_init_param spi_engine_offload_init_param;
	struct spi_engine_offload_message msg;

	uint8_t commands_data[2] = {0xFF, 0xFF};
	int32_t ret, data;
	uint32_t i;

	print("Test\n\r");

	uint32_t spi_eng_msg_cmds[3] = {
		CS_LOW,
		READ(2),
		CS_HIGH
	};

	Xil_ICacheEnable();
	Xil_DCacheEnable();

	ret = ad400x_init(&dev, ad400x_init_param);
	if (ret < 0)
		return ret;

	if (SPI_ENGINE_OFFLOAD_EXAMPLE == 0) {
		while(1) {
			ad400x_spi_single_conversion(dev, &adc_data);
			xil_printf("ADC: %d\n\r", adc_data);
		}
	}
	/* Offload example */
	else {
		ret = spi_engine_offload_init(dev->spi_desc, &spi_engine_offload_init_param);
		if (ret != SUCCESS)
			return FAILURE;

		msg.commands = spi_eng_msg_cmds;
		msg.no_commands = ARRAY_SIZE(spi_eng_msg_cmds);
		msg.rx_addr = 0x800000;
		msg.tx_addr = 0xA000000;
		msg.commands_data = commands_data;

		ret = spi_engine_offload_transfer(dev->spi_desc, msg, AD400x_EVB_SAMPLE_NO);
		if (ret != SUCCESS)
			return ret;

		mdelay(2000);

		offload_data = (uint32_t *)msg.rx_addr;

		for(i = 0; i < AD400x_EVB_SAMPLE_NO; i++) {
			data = *offload_data & 0xFFFFF;
			if (data > 524287)
				data = data - 1048576;
			printf("ADC%d: %d\n", i, data);
			offload_data += 1;
		}
	}

	print("Success\n\r");

	Xil_DCacheDisable();
	Xil_ICacheDisable();
}
