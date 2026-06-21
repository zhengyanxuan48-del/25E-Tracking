
/***************************************************************************************
  * �������ɽ�Э�Ƽ���������ѿ�Դ����?
  * ���������鿴��ʹ�ú��޸ģ���Ӧ�õ��Լ�����Ŀ֮��
  * �����Ȩ�齭Э�Ƽ����У��κ��˻���֯���ý����Ϊ����
  * 
  * �������ƣ�				0.96��OLED��ʾ����������4���I2C�ӿڣ�
  * ���򴴽�ʱ�䣺			2023.10.24
  * ��ǰ����汾��?		V2.0
  * ��ǰ�汾����ʱ�䣺		2024.10.20
  * 
  * ��Э�Ƽ��ٷ���վ��		jiangxiekeji.com
  * ��Э�Ƽ��ٷ��Ա��꣺	jiangxiekeji.taobao.com
  * ������ܼ����¶�̬��?jiangxiekeji.com/tutorial/oled.html
  * 
  * ����㷢�ֳ����е�©�����߱��󣬿�ͨ���ʼ������Ƿ�����feedback@jiangxiekeji.com
  * �����ʼ�֮ǰ��������ȵ����¶�̬ҳ��鿴���³�������������Ѿ��޸ģ��������ٷ��ʼ�?
  ***************************************************************************************
  */
#include "OLED.h"
#include "debug_uart.h"
#include "platform_i2c.h"
#include "ti_msp_dl_config.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

/**
  * ���ݴ洢��ʽ��
  * ����8�㣬��λ���£��ȴ����ң��ٴ��ϵ���
  * ÿһ��Bit��Ӧһ�����ص�
  * 
  *      B0 B0                  B0 B0
  *      B1 B1                  B1 B1
  *      B2 B2                  B2 B2
  *      B3 B3  ------------->  B3 B3 --
  *      B4 B4                  B4 B4  |
  *      B5 B5                  B5 B5  |
  *      B6 B6                  B6 B6  |
  *      B7 B7                  B7 B7  |
  *                                    |
  *  -----------------------------------
  *  |   
  *  |   B0 B0                  B0 B0
  *  |   B1 B1                  B1 B1
  *  |   B2 B2                  B2 B2
  *  --> B3 B3  ------------->  B3 B3
  *      B4 B4                  B4 B4
  *      B5 B5                  B5 B5
  *      B6 B6                  B6 B6
  *      B7 B7                  B7 B7
  * 
  * �����ᶨ�壺
  * ���Ͻ�Ϊ(0, 0)��
  * ��������ΪX�ᣬȡֵ��Χ��0~127
  * ��������ΪY�ᣬȡֵ��Χ��0~63
  * 
  *       0             X��           127 
  *      .------------------------------->
  *    0 |
  *      |
  *      |
  *      |
  *  Y�� |
  *      |
  *      |
  *      |
  *   63 |
  *      v
  * 
  */


/*ȫ�ֱ���*********************/

/**
  * OLED�Դ�����
  * ���е���ʾ��������ֻ�ǶԴ��Դ�������ж��?
  * ������OLED_Update������OLED_UpdateArea����
  * �ŻὫ�Դ���������ݷ��͵�OLEDӲ����������ʾ
  */
uint8_t OLED_DisplayBuf[8][128];

#define OLED_DATA_CHUNK_SIZE 16U

static void OLED_DelayMs(uint32_t ms)
{
    delay_cycles((CPUCLK_FREQ / 1000U) * ms);
}
/*********************ȫ�ֱ���*/


/*��������*********************/

/**
  * ��    ����OLEDдSCL�ߵ͵�ƽ
  * ��    ����Ҫд��SCL�ĵ�ƽֵ����Χ��0/1
  * �� �� ֵ����
  * ˵    �������ϲ㺯����ҪдSCLʱ���˺����ᱻ����
  *           �û���Ҫ���ݲ��������ֵ����SCL��Ϊ�ߵ�ƽ���ߵ͵�ƽ
  *           ����������0ʱ����SCLΪ�͵�ƽ������������1ʱ����SCLΪ�ߵ�ƽ
  */
// PB9 = SCL
void OLED_W_SCL(uint8_t BitValue)
{
    (void) BitValue;
}

// PB8 = SDA
void OLED_W_SDA(uint8_t BitValue)
{
    (void) BitValue;
}
/**
  * ��    ����OLED���ų�ʼ��
  * ��    ������
  * �� �� ֵ����
  * ˵    �������ϲ㺯����Ҫ��ʼ��ʱ���˺����ᱻ����
  *           �û���Ҫ��SCL��SDA���ų�ʼ��Ϊ��©ģʽ�����ͷ�����
  */
void OLED_GPIO_Init(void)
{
	OLED_DelayMs(100);
	platform_i2c_init();
}

/*********************��������*/


/*ͨ��Э��*********************/

/**
  * ��    ����I2C��ʼ
  * ��    ������
  * �� �� ֵ����
  */
void OLED_I2C_Start(void)
{
}

void OLED_I2C_Stop(void)
{
}

void OLED_I2C_SendByte(uint8_t Byte)
{
	(void) Byte;
}

platform_i2c_status_t OLED_WriteDataBufferChunk(
	const uint8_t *data, uint16_t len);
platform_i2c_status_t OLED_WriteDataBuffer(const uint8_t *data, uint16_t len);
/**
  * ��    ����I2C��ֹ
  * ��    ������
  * �� �� ֵ����
  */

platform_i2c_status_t OLED_WriteCommands(const uint8_t *cmds, uint16_t len)
{
	return platform_i2c_write_control_bytes(
		PLATFORM_I2C_ADDR_OLED, 0x00U, cmds, len);
}

platform_i2c_status_t OLED_WriteCommand(uint8_t cmd)
{
	return OLED_WriteCommands(&cmd, 1U);
}

platform_i2c_status_t OLED_WriteCommand2(uint8_t cmd, uint8_t arg)
{
	uint8_t payload[2];

	payload[0] = cmd;
	payload[1] = arg;

	return OLED_WriteCommands(payload, (uint16_t) sizeof(payload));
}

platform_i2c_status_t OLED_WriteData(uint8_t data)
{
	uint8_t buf[2];

	buf[0] = 0x40U;
	buf[1] = data;

	return platform_i2c_write_bytes(
		PLATFORM_I2C_ADDR_OLED, buf, (uint16_t) sizeof(buf));
}

platform_i2c_status_t OLED_WriteDataBufferChunk(
	const uint8_t *data, uint16_t len)
{
	uint8_t tx_buffer[OLED_DATA_CHUNK_SIZE + 1U];
	uint16_t index;

	if ((data == NULL) || (len == 0U) || (len > OLED_DATA_CHUNK_SIZE)) {
		return PLATFORM_I2C_BAD_PARAM;
	}

	tx_buffer[0] = 0x40U;
	for (index = 0U; index < len; index++) {
		tx_buffer[index + 1U] = data[index];
	}

	return platform_i2c_write_bytes(
		PLATFORM_I2C_ADDR_OLED, tx_buffer, (uint16_t)(len + 1U));
}

platform_i2c_status_t OLED_WriteDataBuffer(const uint8_t *data, uint16_t len)
{
	uint16_t offset;

	if ((data == NULL) && (len > 0U)) {
		return PLATFORM_I2C_BAD_PARAM;
	}

	for (offset = 0U; offset < len; offset += OLED_DATA_CHUNK_SIZE) {
		uint16_t count = (uint16_t)(len - offset);
		platform_i2c_status_t status;

		if (count > OLED_DATA_CHUNK_SIZE) {
			count = OLED_DATA_CHUNK_SIZE;
		}

		status = OLED_WriteDataBufferChunk(&data[offset], count);
		if (status != PLATFORM_I2C_OK) {
			return status;
		}
	}

	return PLATFORM_I2C_OK;
}

static void OLED_LogStage(const char *stage, platform_i2c_status_t status)
{
	if (status != PLATFORM_I2C_OK) {
		debug_printf("[OLED] %s fail\r\n", stage);
	}
}

static platform_i2c_status_t OLED_SetCursorChecked(uint8_t Page, uint8_t X)
{
	uint8_t commands[3];

	commands[0] = (uint8_t)(0xB0U | Page);
	commands[1] = (uint8_t)(X & 0x0FU);
	commands[2] = (uint8_t)(0x10U | ((X & 0xF0U) >> 4));

	return OLED_WriteCommands(commands, (uint16_t) sizeof(commands));
}

static platform_i2c_status_t OLED_UpdateChecked(uint8_t log_clear_fail)
{
	uint8_t page;

	for (page = 0U; page < 8U; page++) {
		uint8_t x;

		for (x = 0U; x < 128U; x += OLED_DATA_CHUNK_SIZE) {
			platform_i2c_status_t status;
			uint8_t count = OLED_DATA_CHUNK_SIZE;
			uint8_t chunk = (uint8_t)(x / OLED_DATA_CHUNK_SIZE);

			if ((uint8_t)(128U - x) < count) {
				count = (uint8_t)(128U - x);
			}

			status = OLED_SetCursorChecked(page, x);
			if (status != PLATFORM_I2C_OK) {
				if (log_clear_fail != 0U) {
					debug_printf("[OLED] clear page=%u chunk=%u fail\r\n",
						(unsigned int) page, (unsigned int) chunk);
				}
				return status;
			}

			status = OLED_WriteDataBufferChunk(&OLED_DisplayBuf[page][x], count);
			if (status != PLATFORM_I2C_OK) {
				if (log_clear_fail != 0U) {
					debug_printf("[OLED] clear page=%u chunk=%u fail\r\n",
						(unsigned int) page, (unsigned int) chunk);
				}
				return status;
			}

		}
	}

	return PLATFORM_I2C_OK;
}

/**
  * ��    ����OLEDд����
  * ��    ����Data Ҫд�����ݵ���ʼ��ַ
  * ��    ����Count Ҫд�����ݵ�����
  * �� �� ֵ����
  */

/*********************ͨ��Э��*/


/*Ӳ������*********************/

/**
  * ��    ����OLED��ʼ��
  * ��    ������
  * �� �� ֵ����
  * ˵    ����ʹ��ǰ����Ҫ���ô˳�ʼ������
  */
platform_i2c_status_t OLED_InitStatus(void)
{
	static const uint8_t init_commands_before_charge_pump[] = {
		0xD5U, 0x80U,
		0xA8U, 0x3FU,
		0xD3U, 0x00U,
		0x40U,
		0xA1U,
		0xC8U,
		0xDAU, 0x12U,
		0x81U, 0xCFU,
		0xD9U, 0xF1U,
		0xDBU, 0x30U,
		0xA4U,
		0xA6U
	};
	platform_i2c_status_t status;
	platform_i2c_status_t init_status = PLATFORM_I2C_OK;

	OLED_DelayMs(100);
	OLED_GPIO_Init();

	status = OLED_WriteCommand(0xAEU);
	OLED_LogStage("display off", status);
	if (status != PLATFORM_I2C_OK) {
		init_status = status;
	}

	status = OLED_WriteCommand2(0x20U, 0x02U);
	if (status != PLATFORM_I2C_OK) {
		debug_print("[OLED] cmd fail cmd=0x20 arg=0x02\r\n");
		if (init_status == PLATFORM_I2C_OK) {
			init_status = status;
		}
	}

	status = OLED_WriteCommands(init_commands_before_charge_pump,
		(uint16_t) sizeof(init_commands_before_charge_pump));
	if (status != PLATFORM_I2C_OK) {
		debug_print("[OLED] init command block fail\r\n");
		if (init_status == PLATFORM_I2C_OK) {
			init_status = status;
		}
	}

	status = OLED_WriteCommand2(0x8DU, 0x14U);
	if (status != PLATFORM_I2C_OK) {
		debug_print("[OLED] cmd fail cmd=0x8D arg=0x14\r\n");
		if (init_status == PLATFORM_I2C_OK) {
			init_status = status;
		}
	}
	OLED_LogStage("charge pump", status);

	status = OLED_WriteCommand(0xAFU);
	OLED_LogStage("display on", status);
	if ((status != PLATFORM_I2C_OK) && (init_status == PLATFORM_I2C_OK)) {
		init_status = status;
	}

	if (init_status == PLATFORM_I2C_OK) {
		debug_print("[OLED] init ok\r\n");
	} else {
		debug_print("[OLED] init fail\r\n");
	}

	return init_status;
}

void OLED_Init(void)
{
	(void) OLED_InitStatus();
}
/**
  * ��    ����OLED������ʾ���λ��?
  * ��    ����Page ָ��������ڵ�ҳ����Χ��?~7
  * ��    ����X ָ��������ڵ�X�����꣬��Χ��0~127
  * �� �� ֵ����
  * ˵    ����OLEDĬ�ϵ�Y�ᣬֻ��8��BitΪһ��д�룬��1ҳ����8��Y������
  */
void OLED_SetCursor(uint8_t Page, uint8_t X)
{
	(void) OLED_SetCursorChecked(Page, X);
}

/*********************Ӳ������*/


/*���ߺ���*********************/

/*���ߺ��������ڲ����ֺ���ʹ��*/

/**
  * ��    �����η�����
  * ��    ����X ����
  * ��    ����Y ָ��
  * �� �� ֵ������X��Y�η�
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;	//���Ĭ���?
	while (Y --)			//�۳�Y��
	{
		Result *= X;		//ÿ�ΰ�X�۳˵������?
	}
	return Result;
}

/**
  * ��    �����ж�ָ�����Ƿ���ָ��������ڲ�?
  * ��    ����nvert ����εĶ�����?
  * ��    ����vertx verty ��������ζ����x��y���������?
  * ��    ����testx testy ���Ե��X��y����
  * �� �� ֵ��ָ�����Ƿ���ָ��������ڲ���?�����ڲ���0�������ڲ�
  */
uint8_t OLED_pnpoly(uint8_t nvert, int16_t *vertx, int16_t *verty, int16_t testx, int16_t testy)
{
	int16_t i, j, c = 0;
	
	/* Comment removed: source encoding was damaged. */
	/*�ο����ӣ�https://wrfranklin.org/Research/Short_Notes/pnpoly.html*/
	for (i = 0, j = nvert - 1; i < nvert; j = i++)
	{
		if (((verty[i] > testy) != (verty[j] > testy)) &&
			(testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
		{
			c = !c;
		}
	}
	return c;
}

/**
  * ��    �����ж�ָ�����Ƿ���ָ���Ƕ��ڲ�
  * ��    ����X Y ָ���������?
  * ��    ����StartAngle EndAngle ��ʼ�ǶȺ���ֹ�Ƕȣ���Χ��-180~180
  *           ˮƽ����Ϊ0�ȣ�ˮƽ����Ϊ180�Ȼ�-180�ȣ��·�Ϊ�������Ϸ�Ϊ������˳ʱ����ת
  * �� �� ֵ��ָ�����Ƿ���ָ���Ƕ��ڲ���1�����ڲ���0�������ڲ�
  */
uint8_t OLED_IsInAngle(int16_t X, int16_t Y, int16_t StartAngle, int16_t EndAngle)
{
	int16_t PointAngle;
	PointAngle = atan2(Y, X) / 3.14 * 180;	//����ָ����Ļ��ȣ���ת��Ϊ�Ƕȱ��?
	if (StartAngle < EndAngle)	//��ʼ�Ƕ�С����ֹ�Ƕȵ����?
	{
		/* Comment removed: source encoding was damaged. */
		if (PointAngle >= StartAngle && PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	else			//��ʼ�Ƕȴ�������ֹ�Ƕȵ����?
	{
		/* Comment removed: source encoding was damaged. */
		if (PointAngle >= StartAngle || PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	return 0;		//�������������������ж��ж�ָ���㲻��ָ���Ƕ�
}

/*********************���ߺ���*/


/*���ܺ���*********************/

/**
  * ��    ������OLED�Դ�������µ�OLED��Ļ
  * ��    ������
  * �� �� ֵ����
  * ˵    �������е���ʾ��������ֻ�Ƕ�OLED�Դ�������ж��?
  *           ������OLED_Update������OLED_UpdateArea����
  *           �ŻὫ�Դ���������ݷ��͵�OLEDӲ����������ʾ
  *           �ʵ�����ʾ������Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
platform_i2c_status_t OLED_UpdateStatus(void)
{
	return OLED_UpdateChecked(0U);
}

void OLED_Update(void)
{
	(void) OLED_UpdateStatus();
}
/**
  * ��    ������OLED�Դ����鲿�ָ��µ�OLED��Ļ
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Width ָ������Ŀ��ȣ���Χ��?~128
  * ��    ����Height ָ������ĸ߶ȣ���Χ��?~64
  * �� �� ֵ����
  * ˵    �����˺��������ٸ��²���ָ��������
  *           �����������Y��ֻ��������ҳ����ͬһҳ��ʣ�ಿ�ֻ����һ�����
  * ˵    �������е���ʾ��������ֻ�Ƕ�OLED�Դ�������ж��?
  *           ������OLED_Update������OLED_UpdateArea����
  *           �ŻὫ�Դ���������ݷ��͵�OLEDӲ����������ʾ
  *           �ʵ�����ʾ������Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
platform_i2c_status_t OLED_UpdateAreaStatus(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t j;
	int16_t Page, Page1;

	if ((Width == 0U) || (Height == 0U)) {
		return PLATFORM_I2C_OK;
	}

	if (X < 0) {
		if (((int16_t) Width + X) <= 0) {
			return PLATFORM_I2C_OK;
		}
		Width = (uint8_t)((int16_t) Width + X);
		X = 0;
	}

	if (X > 127) {
		return PLATFORM_I2C_OK;
	}

	if (((int16_t) X + (int16_t) Width) > 128) {
		Width = (uint8_t)(128 - X);
	}

	Page = Y / 8;
	Page1 = (Y + Height - 1) / 8 + 1;
	if (Y < 0) {
		Page -= 1;
		Page1 -= 1;
	}

	for (j = Page; j < Page1; j++) {
		if ((j >= 0) && (j <= 7)) {
			platform_i2c_status_t status;

			status = OLED_SetCursorChecked((uint8_t) j, (uint8_t) X);
			if (status != PLATFORM_I2C_OK) {
				return status;
			}

			status = OLED_WriteDataBuffer(&OLED_DisplayBuf[j][X], Width);
			if (status != PLATFORM_I2C_OK) {
				return status;
			}
		}
	}

	return PLATFORM_I2C_OK;
}

void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	(void) OLED_UpdateAreaStatus(X, Y, Width, Height);
}
/**
  * ��    ������OLED�Դ�����ȫ������
  * ��    ������
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_Clear(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)				//����8ҳ
	{
		for (i = 0; i < 128; i ++)			//����128��
		{
			OLED_DisplayBuf[j][i] = 0x00;	//���Դ���������ȫ������
		}
	}
}

/**
  * ��    ������OLED�Դ����鲿������
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Width ָ������Ŀ��ȣ���Χ��?~128
  * ��    ����Height ָ������ĸ߶ȣ���Χ��?~64
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t i, j;
	
	for (j = Y; j < Y + Height; j ++)		//����ָ��ҳ
	{
		for (i = X; i < X + Width; i ++)	//����ָ����
		{
			if (i >= 0 && i <= 127 && j >=0 && j <= 63)				//������Ļ�����ݲ���ʾ
			{
				OLED_DisplayBuf[j / 8][i] &= ~(0x01 << (j % 8));	//���Դ�����ָ����������
			}
		}
	}
}

/**
  * ��    ������OLED�Դ�����ȫ��ȡ��
  * ��    ������
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_Reverse(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)				//����8ҳ
	{
		for (i = 0; i < 128; i ++)			//����128��
		{
			OLED_DisplayBuf[j][i] ^= 0xFF;	//���Դ���������ȫ��ȡ��
		}
	}
}
	
/**
  * ��    ������OLED�Դ����鲿��ȡ��
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Width ָ������Ŀ��ȣ���Χ��?~128
  * ��    ����Height ָ������ĸ߶ȣ���Χ��?~64
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t i, j;
	
	for (j = Y; j < Y + Height; j ++)		//����ָ��ҳ
	{
		for (i = X; i < X + Width; i ++)	//����ָ����
		{
			if (i >= 0 && i <= 127 && j >=0 && j <= 63)			//������Ļ�����ݲ���ʾ
			{
				OLED_DisplayBuf[j / 8][i] ^= 0x01 << (j % 8);	//���Դ�����ָ������ȡ��
			}
		}
	}
}

/**
  * ��    ����OLED��ʾһ���ַ�
  * ��    ����X ָ���ַ����Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���ַ����Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Char ָ��Ҫ��ʾ���ַ�����Χ��ASCII��ɼ��ַ�?
  * ��    ����FontSize ָ��������?
  *           ��Χ��OLED_8X16		��8���أ���16����
  *                 OLED_6X8		��6���أ���8����
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize)
{
	if (FontSize == OLED_8X16)		//����Ϊ��8���أ���16����
	{
		/* Comment removed: source encoding was damaged. */
		OLED_ShowImage(X, Y, 8, 16, OLED_F8x16[Char - ' ']);
	}
	else if(FontSize == OLED_6X8)	//����Ϊ��6���أ���8����
	{
		/* Comment removed: source encoding was damaged. */
		OLED_ShowImage(X, Y, 6, 8, OLED_F6x8[Char - ' ']);
	}
}

/**
  * ��    ����OLED��ʾ�ַ�����֧��ASCII������Ļ��д�룩
  * ��    ����X ָ���ַ������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���ַ������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����String ָ��Ҫ��ʾ���ַ�������Χ��ASCII��ɼ��ַ��������ַ���ɵ��ַ���
  * ��    ����FontSize ָ��������?
  *           ��Χ��OLED_8X16		��8���أ���16����
  *                 OLED_6X8		��6���أ���8����
  * �� �� ֵ����
  * ˵    ������ʾ�������ַ���Ҫ��OLED_Data.c���OLED_CF16x16���鶨��
  *           δ�ҵ�ָ�������ַ�ʱ������ʾĬ��ͼ�Σ�һ�������ڲ�һ���ʺţ�
  *           �������СΪOLED_8X16ʱ�������ַ���16*16����������ʾ
  *           �������СΪOLED_6X8ʱ�������ַ���6*8������ʾ'?'
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ShowString(uint8_t X, uint8_t Y, const char *String, uint8_t FontSize)
{
	uint16_t i = 0;
	char SingleChar[5];
	uint8_t CharLength = 0;
	uint16_t XOffset = 0;
	uint16_t pIndex;
	
	while (String[i] != '\0')	//�����ַ���
	{
		
#ifdef OLED_CHARSET_UTF8						//�����ַ���ΪUTF8
		/*�˶δ����Ŀ���ǣ���ȡUTF8�ַ����е�һ���ַ���ת�浽SingleChar���ַ�����*/
		/* Comment removed: source encoding was damaged. */
		if ((String[i] & 0x80) == 0x00)			//��һ���ֽ�Ϊ0xxxxxxx
		{
			CharLength = 1;						//�ַ�Ϊ1�ֽ�
			SingleChar[0] = String[i ++];		//����һ���ֽ�д��SingleChar��0��λ�ã����iָ����һ���ֽ�
			SingleChar[1] = '\0';				//ΪSingleChar�����ַ���������־λ
		}
		else if ((String[i] & 0xE0) == 0xC0)	//��һ���ֽ�Ϊ110xxxxx
		{
			CharLength = 2;						//�ַ�Ϊ2�ֽ�
			SingleChar[0] = String[i ++];		//����һ���ֽ�д��SingleChar��0��λ�ã����iָ����һ���ֽ�
			if (String[i] == '\0') {break;}		//�������������ѭ�����������?
			SingleChar[1] = String[i ++];		//���ڶ����ֽ�д��SingleChar��1��λ�ã����iָ����һ���ֽ�
			SingleChar[2] = '\0';				//ΪSingleChar�����ַ���������־λ
		}
		else if ((String[i] & 0xF0) == 0xE0)	//��һ���ֽ�Ϊ1110xxxx
		{
			CharLength = 3;						//�ַ�Ϊ3�ֽ�
			SingleChar[0] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[1] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[2] = String[i ++];
			SingleChar[3] = '\0';
		}
		else if ((String[i] & 0xF8) == 0xF0)	//��һ���ֽ�Ϊ11110xxx
		{
			CharLength = 4;						//�ַ�Ϊ4�ֽ�
			SingleChar[0] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[1] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[2] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[3] = String[i ++];
			SingleChar[4] = '\0';
		}
		else
		{
			i ++;			//���������iָ����һ���ֽڣ����Դ��ֽڣ������ж���һ���ֽ�
			continue;
		}
#endif
		
#ifdef OLED_CHARSET_GB2312						//�����ַ���ΪGB2312
		/*�˶δ����Ŀ���ǣ���ȡGB2312�ַ����е�һ���ַ���ת�浽SingleChar���ַ�����*/
		/* Comment removed: source encoding was damaged. */
		if ((String[i] & 0x80) == 0x00)			//���λ�?
		{
			CharLength = 1;						//�ַ�Ϊ1�ֽ�
			SingleChar[0] = String[i ++];		//����һ���ֽ�д��SingleChar��0��λ�ã����iָ����һ���ֽ�
			SingleChar[1] = '\0';				//ΪSingleChar�����ַ���������־λ
		}
		else									//���λ�?
		{
			CharLength = 2;						//�ַ�Ϊ2�ֽ�
			SingleChar[0] = String[i ++];		//����һ���ֽ�д��SingleChar��0��λ�ã����iָ����һ���ֽ�
			if (String[i] == '\0') {break;}		//�������������ѭ�����������?
			SingleChar[1] = String[i ++];		//���ڶ����ֽ�д��SingleChar��1��λ�ã����iָ����һ���ֽ�
			SingleChar[2] = '\0';				//ΪSingleChar�����ַ���������־λ
		}
#endif
		
		/*��ʾ����������ȡ����SingleChar*/
		if (CharLength == 1)	//����ǵ��ֽ��ַ�?
		{
			/*ʹ��OLED_ShowChar��ʾ���ַ�*/
			OLED_ShowChar(X + XOffset, Y, SingleChar[0], FontSize);
			XOffset += FontSize;
		}
		else					//���򣬼����ֽ��ַ�
		{
			/*����������ģ�⣬����ģ����Ѱ�Ҵ��ַ�������*/
			/* Comment removed: source encoding was damaged. */
			for (pIndex = 0; strcmp(OLED_CF16x16[pIndex].Index, "") != 0; pIndex ++)
			{
				/* Comment removed: source encoding was damaged. */
				if (strcmp(OLED_CF16x16[pIndex].Index, SingleChar) == 0)
				{
					break;		//����ѭ������ʱpIndex��ֵΪָ���ַ�������
				}
			}
			if (FontSize == OLED_8X16)		//��������Ϊ8*16����
			{
				/* Comment removed: source encoding was damaged. */
				OLED_ShowImage(X + XOffset, Y, 16, 16, OLED_CF16x16[pIndex].Data);
				XOffset += 16;
			}
			else if (FontSize == OLED_6X8)	//��������Ϊ6*8����
			{
				/*�ռ䲻�㣬��λ����ʾ'?'*/
				OLED_ShowChar(X + XOffset, Y, '?', OLED_6X8);
				XOffset += OLED_6X8;
			}
		}
	}
}

/**
  * ��    ����OLED��ʾ���֣�ʮ���ƣ���������
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Number ָ��Ҫ��ʾ�����֣���Χ��0~4294967295
  * ��    ����Length ָ�����ֵĳ��ȣ���Χ��0~10
  * ��    ����FontSize ָ��������?
  *           ��Χ��OLED_8X16		��8���أ���16����
  *                 OLED_6X8		��6���أ���8����
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; i < Length; i++)		//�������ֵ�ÿһλ							
	{
		/*����OLED_ShowChar������������ʾÿ������*/
		/*Number / OLED_Pow(10, Length - i - 1) % 10 ����ʮ������ȡ���ֵ�ÿһλ*/
		/*+ '0' �ɽ�����ת��Ϊ�ַ���ʽ*/
		OLED_ShowChar(X + i * FontSize, Y, Number / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize);
	}
}

/**
  * ��    ����OLED��ʾ�з������֣�ʮ���ƣ�������
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Number ָ��Ҫ��ʾ�����֣���Χ��-2147483648~2147483647
  * ��    ����Length ָ�����ֵĳ��ȣ���Χ��0~10
  * ��    ����FontSize ָ��������?
  *           ��Χ��OLED_8X16		��8���أ���16����
  *                 OLED_6X8		��6���أ���8����
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	uint32_t Number1;
	
	if (Number >= 0)						//���ִ��ڵ���0
	{
		OLED_ShowChar(X, Y, '+', FontSize);	//��ʾ+��
		Number1 = Number;					//Number1ֱ�ӵ���Number
	}
	else									//����С��0
	{
		OLED_ShowChar(X, Y, '-', FontSize);	//��ʾ-��
		Number1 = -Number;					//Number1����Numberȡ��
	}
	
	for (i = 0; i < Length; i++)			//�������ֵ�ÿһλ								
	{
		/*����OLED_ShowChar������������ʾÿ������*/
		/*Number1 / OLED_Pow(10, Length - i - 1) % 10 ����ʮ������ȡ���ֵ�ÿһλ*/
		/*+ '0' �ɽ�����ת��Ϊ�ַ���ʽ*/
		OLED_ShowChar(X + (i + 1) * FontSize, Y, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize);
	}
}

/**
  * ��    ����OLED��ʾʮ���������֣�ʮ�����ƣ���������
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Number ָ��Ҫ��ʾ�����֣���Χ��0x00000000~0xFFFFFFFF
  * ��    ����Length ָ�����ֵĳ��ȣ���Χ��0~8
  * ��    ����FontSize ָ��������?
  *           ��Χ��OLED_8X16		��8���أ���16����
  *                 OLED_6X8		��6���أ���8����
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)		//�������ֵ�ÿһλ
	{
		/*��ʮ��������ȡ���ֵ�ÿһλ*/
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		
		if (SingleNumber < 10)			//��������С��10
		{
			/*����OLED_ShowChar��������ʾ������*/
			/*+ '0' �ɽ�����ת��Ϊ�ַ���ʽ*/
			OLED_ShowChar(X + i * FontSize, Y, SingleNumber + '0', FontSize);
		}
		else							//�������ִ���10
		{
			/*����OLED_ShowChar��������ʾ������*/
			/*+ 'A' �ɽ�����ת��Ϊ��A��ʼ��ʮ�������ַ�*/
			OLED_ShowChar(X + i * FontSize, Y, SingleNumber - 10 + 'A', FontSize);
		}
	}
}

/**
  * ��    ����OLED��ʾ���������֣������ƣ���������
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Number ָ��Ҫ��ʾ�����֣���Χ��0x00000000~0xFFFFFFFF
  * ��    ����Length ָ�����ֵĳ��ȣ���Χ��0~16
  * ��    ����FontSize ָ��������?
  *           ��Χ��OLED_8X16		��8���أ���16����
  *                 OLED_6X8		��6���أ���8����
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; i < Length; i++)		//�������ֵ�ÿһλ	
	{
		/*����OLED_ShowChar������������ʾÿ������*/
		/*Number / OLED_Pow(2, Length - i - 1) % 2 ���Զ�������ȡ���ֵ�ÿһλ*/
		/*+ '0' �ɽ�����ת��Ϊ�ַ���ʽ*/
		OLED_ShowChar(X + i * FontSize, Y, Number / OLED_Pow(2, Length - i - 1) % 2 + '0', FontSize);
	}
}

/**
  * ��    ����OLED��ʾ�������֣�ʮ���ƣ�С����
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Number ָ��Ҫ��ʾ�����֣���Χ��-4294967295.0~4294967295.0
  * ��    ����IntLength ָ�����ֵ�����λ���ȣ���Χ��0~10
  * ��    ����FraLength ָ�����ֵ�С��λ���ȣ���Χ��0~9��С����������������ʾ
  * ��    ����FontSize ָ��������?
  *           ��Χ��OLED_8X16		��8���أ���16����
  *                 OLED_6X8		��6���أ���8����
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
	uint32_t PowNum, IntNum, FraNum;
	
	if (Number >= 0)						//���ִ��ڵ���0
	{
		OLED_ShowChar(X, Y, '+', FontSize);	//��ʾ+��
	}
	else									//����С��0
	{
		OLED_ShowChar(X, Y, '-', FontSize);	//��ʾ-��
		Number = -Number;					//Numberȡ��
	}
	
	/*��ȡ�������ֺ�С������*/
	IntNum = Number;						//ֱ�Ӹ�ֵ�����ͱ�������ȡ����
	Number -= IntNum;						//��Number��������������ֹ֮��С���˵�����ʱ����������ɴ���?
	PowNum = OLED_Pow(10, FraLength);		//����ָ��С����λ����ȷ������
	FraNum = round(Number * PowNum);		//��С���˵�������ͬʱ�������룬������ʾ���?
	IntNum += FraNum / PowNum;				//��������������˽�λ������Ҫ�ټӸ�����?
	
	/*��ʾ��������*/
	OLED_ShowNum(X + FontSize, Y, IntNum, IntLength, FontSize);
	
	/*��ʾС����*/
	OLED_ShowChar(X + (IntLength + 1) * FontSize, Y, '.', FontSize);
	
	/*��ʾС������*/
	OLED_ShowNum(X + (IntLength + 2) * FontSize, Y, FraNum, FraLength, FontSize);
}

/**
  * ��    ����OLED��ʾͼ��
  * ��    ����X ָ��ͼ�����Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ��ͼ�����Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Width ָ��ͼ��Ŀ��ȣ���Χ��?~128
  * ��    ����Height ָ��ͼ��ĸ߶ȣ���Χ��?~64
  * ��    ����Image ָ��Ҫ��ʾ��ͼ��
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
	uint8_t i = 0, j = 0;
	int16_t Page, Shift;
	
	/* Comment removed: source encoding was damaged. */
	OLED_ClearArea(X, Y, Width, Height);
	
	/* Comment removed: source encoding was damaged. */
	/*(Height - 1) / 8 + 1��Ŀ����Height / 8������ȡ��*/
	for (j = 0; j < (Height - 1) / 8 + 1; j ++)
	{
		/* Comment removed: source encoding was damaged. */
		for (i = 0; i < Width; i ++)
		{
			if (X + i >= 0 && X + i <= 127)		//������Ļ�����ݲ���ʾ
			{
				/*���������ڼ���ҳ��ַ����λʱ��Ҫ��һ��ƫ��*/
				Page = Y / 8;
				Shift = Y % 8;
				if (Y < 0)
				{
					Page -= 1;
					Shift += 8;
				}
				
				if (Page + j >= 0 && Page + j <= 7)		//������Ļ�����ݲ���ʾ
				{
					/*��ʾͼ���ڵ�ǰҳ������*/
					OLED_DisplayBuf[Page + j][X + i] |= Image[j * Width + i] << (Shift);
				}
				
				if (Page + j + 1 >= 0 && Page + j + 1 <= 7)		//������Ļ�����ݲ���ʾ
				{					
					/*��ʾͼ������һҳ������*/
					OLED_DisplayBuf[Page + j + 1][X + i] |= Image[j * Width + i] >> (8 - Shift);
				}
			}
		}
	}
}

/**
  * ��    ����OLEDʹ��printf������ӡ��ʽ���ַ�����֧��ASCII������Ļ��д�룩
  * ��    ����X ָ����ʽ���ַ������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ����ʽ���ַ������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����FontSize ָ��������?
  *           ��Χ��OLED_8X16		��8���أ���16����
  *                 OLED_6X8		��6���أ���8����
  * ��    ����format ָ��Ҫ��ʾ�ĸ�ʽ���ַ�������Χ��ASCII��ɼ��ַ��������ַ���ɵ��ַ���
  * ��    ����... ��ʽ���ַ��������б�
  * �� �� ֵ����
  * ˵    ������ʾ�������ַ���Ҫ��OLED_Data.c���OLED_CF16x16���鶨��
  *           δ�ҵ�ָ�������ַ�ʱ������ʾĬ��ͼ�Σ�һ�������ڲ�һ���ʺţ�
  *           �������СΪOLED_8X16ʱ�������ַ���16*16����������ʾ
  *           �������СΪOLED_6X8ʱ�������ַ���6*8������ʾ'?'
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...)
{
	char String[256];						//�����ַ�����
	va_list arg;							//����ɱ�����б��������͵ı���arg
	va_start(arg, format);					//��format��ʼ�����ղ����б���arg����
	vsprintf(String, format, arg);			//ʹ��vsprintf��ӡ��ʽ���ַ����Ͳ����б����ַ�������
	va_end(arg);							//��������arg
	OLED_ShowString(X, Y, String, FontSize);//OLED��ʾ�ַ����飨�ַ�����
}

/**
  * ��    ����OLED��ָ��λ�û�һ����
  * ��    ����X ָ����ĺ����꣬��Χ��?32768~32767����Ļ����0~127
  * ��    ����Y ָ����������꣬��Χ��?32768~32767����Ļ����0~63
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_DrawPoint(int16_t X, int16_t Y)
{
	if (X >= 0 && X <= 127 && Y >=0 && Y <= 63)		//������Ļ�����ݲ���ʾ
	{
		/*���Դ�����ָ��λ�õ�һ��Bit������1*/
		OLED_DisplayBuf[Y / 8][X] |= 0x01 << (Y % 8);
	}
}

/**
  * ��    ����OLED��ȡָ��λ�õ���?
  * ��    ����X ָ����ĺ����꣬��Χ��?32768~32767����Ļ����0~127
  * ��    ����Y ָ����������꣬��Χ��?32768~32767����Ļ����0~63
  * �� �� ֵ��ָ��λ�õ��Ƿ��ڵ���״̬��1��������0��Ϩ��
  */
uint8_t OLED_GetPoint(int16_t X, int16_t Y)
{
	if (X >= 0 && X <= 127 && Y >=0 && Y <= 63)		//������Ļ�����ݲ���ȡ
	{
		/*�ж�ָ��λ�õ�����*/
		if (OLED_DisplayBuf[Y / 8][X] & 0x01 << (Y % 8))
		{
			return 1;	//Ϊ1������1
		}
	}
	
	return 0;		//���򣬷���0
}

/**
  * ��    ����OLED����
  * ��    ����X0 ָ��һ���˵�ĺ����꣬��Χ��?32768~32767����Ļ����0~127
  * ��    ����Y0 ָ��һ���˵�������꣬��Χ��?32768~32767����Ļ����0~63
  * ��    ����X1 ָ����һ���˵�ĺ����꣬��Χ��?32768~32767����Ļ����0~127
  * ��    ����Y1 ָ����һ���˵�������꣬��Χ��?32768~32767����Ļ����0~63
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
	int16_t x, y, dx, dy, d, incrE, incrNE, temp;
	int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
	uint8_t yflag = 0, xyflag = 0;
	
	if (y0 == y1)		//���ߵ�������
	{
		/*0�ŵ�X�������?�ŵ�X���꣬�򽻻�����X����*/
		if (x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
		
		/*����X����*/
		for (x = x0; x <= x1; x ++)
		{
			OLED_DrawPoint(x, y0);	//���λ���
		}
	}
	else if (x0 == x1)	//���ߵ�������
	{
		/*0�ŵ�Y�������?�ŵ�Y���꣬�򽻻�����Y����*/
		if (y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
		
		/*����Y����*/
		for (y = y0; y <= y1; y ++)
		{
			OLED_DrawPoint(x0, y);	//���λ���
		}
	}
	else				//б��
	{
		/* Comment removed: source encoding was damaged. */
		/*�ο��ĵ���https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf*/
		/*�ο��̳̣�https://www.bilibili.com/video/BV1364y1d7Lo*/
		
		if (x0 > x1)	//0�ŵ�X�������?�ŵ�X����
		{
			/*������������*/
			/*������Ӱ�컭�ߣ����ǻ��߷����ɵ�һ���������������ޱ�Ϊ��һ��������*/
			temp = x0; x0 = x1; x1 = temp;
			temp = y0; y0 = y1; y1 = temp;
		}
		
		if (y0 > y1)	//0�ŵ�Y�������?�ŵ�Y����
		{
			/*��Y����ȡ��*/
			/*ȡ����Ӱ�컭�ߣ����ǻ��߷����ɵ�һ�������ޱ�Ϊ��һ����*/
			y0 = -y0;
			y1 = -y1;
			
			/*�ñ�־λyflag����ס��ǰ�任���ں���ʵ�ʻ���ʱ���ٽ����껻����*/
			yflag = 1;
		}
		
		if (y1 - y0 > x1 - x0)	//����б�ʴ���1
		{
			/*��X������Y���껥��*/
			/*������Ӱ�컭�ߣ����ǻ��߷����ɵ�һ����0~90�ȷ�Χ��Ϊ��һ����0~45�ȷ�Χ*/
			temp = x0; x0 = y0; y0 = temp;
			temp = x1; x1 = y1; y1 = temp;
			
			/*�ñ�־λxyflag����ס��ǰ�任���ں���ʵ�ʻ���ʱ���ٽ����껻����*/
			xyflag = 1;
		}
		
		/*����ΪBresenham�㷨��ֱ��*/
		/*�㷨Ҫ�󣬻��߷������Ϊ��һ����?~45�ȷ�Χ*/
		dx = x1 - x0;
		dy = y1 - y0;
		incrE = 2 * dy;
		incrNE = 2 * (dy - dx);
		d = 2 * dy - dx;
		x = x0;
		y = y0;
		
		/*����ʼ�㣬ͬʱ�жϱ�־λ�������껻����*/
		if (yflag && xyflag){OLED_DrawPoint(y, -x);}
		else if (yflag)		{OLED_DrawPoint(x, -y);}
		else if (xyflag)	{OLED_DrawPoint(y, x);}
		else				{OLED_DrawPoint(x, y);}
		
		while (x < x1)		//����X���ÿ����?
		{
			x ++;
			if (d < 0)		//��һ�����ڵ�ǰ�㶫��
			{
				d += incrE;
			}
			else			//��һ�����ڵ�ǰ�㶫����
			{
				y ++;
				d += incrNE;
			}
			
			/*��ÿһ���㣬ͬʱ�жϱ�־λ�������껻����*/
			if (yflag && xyflag){OLED_DrawPoint(y, -x);}
			else if (yflag)		{OLED_DrawPoint(x, -y);}
			else if (xyflag)	{OLED_DrawPoint(y, x);}
			else				{OLED_DrawPoint(x, y);}
		}	
	}
}

/**
  * ��    ����OLED����
  * ��    ����X ָ���������Ͻǵĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ���������Ͻǵ������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Width ָ�����εĿ��ȣ���Χ��0~128
  * ��    ����Height ָ�����εĸ߶ȣ���Χ��0~64
  * ��    ����IsFilled ָ�������Ƿ����?
  *           ��Χ��OLED_UNFILLED		�����?
  *                 OLED_FILLED			���?
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
	int16_t i, j;
	if (!IsFilled)		//ָ�����β����?
	{
		/*��������X���꣬����������������*/
		for (i = X; i < X + Width; i ++)
		{
			OLED_DrawPoint(i, Y);
			OLED_DrawPoint(i, Y + Height - 1);
		}
		/*��������Y���꣬����������������*/
		for (i = Y; i < Y + Height; i ++)
		{
			OLED_DrawPoint(X, i);
			OLED_DrawPoint(X + Width - 1, i);
		}
	}
	else				//ָ���������?
	{
		/*����X����*/
		for (i = X; i < X + Width; i ++)
		{
			/*����Y����*/
			for (j = Y; j < Y + Height; j ++)
			{
				/* Comment removed: source encoding was damaged. */
				OLED_DrawPoint(i, j);
			}
		}
	}
}

/**
  * ��    ����OLED������
  * ��    ����X0 ָ����һ���˵�ĺ����꣬��Χ��?32768~32767����Ļ����0~127
  * ��    ����Y0 ָ����һ���˵�������꣬��Χ��?32768~32767����Ļ����0~63
  * ��    ����X1 ָ���ڶ����˵�ĺ����꣬��Χ��?32768~32767����Ļ����0~127
  * ��    ����Y1 ָ���ڶ����˵�������꣬��Χ��?32768~32767����Ļ����0~63
  * ��    ����X2 ָ���������˵�ĺ����꣬��Χ��?32768~32767����Ļ����0~127
  * ��    ����Y2 ָ���������˵�������꣬��Χ��?32768~32767����Ļ����0~63
  * ��    ����IsFilled ָ���������Ƿ����?
  *           ��Χ��OLED_UNFILLED		�����?
  *                 OLED_FILLED			���?
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled)
{
	int16_t minx = X0, miny = Y0, maxx = X0, maxy = Y0;
	int16_t i, j;
	int16_t vx[] = {X0, X1, X2};
	int16_t vy[] = {Y0, Y1, Y2};
	
	if (!IsFilled)			//ָ�������β����?
	{
		/*���û��ߺ���������������ֱ������*/
		OLED_DrawLine(X0, Y0, X1, Y1);
		OLED_DrawLine(X0, Y0, X2, Y2);
		OLED_DrawLine(X1, Y1, X2, Y2);
	}
	else					//ָ�����������?
	{
		/*�ҵ���������С��X��Y����*/
		if (X1 < minx) {minx = X1;}
		if (X2 < minx) {minx = X2;}
		if (Y1 < miny) {miny = Y1;}
		if (Y2 < miny) {miny = Y2;}
		
		/*�ҵ�����������X��Y����*/
		if (X1 > maxx) {maxx = X1;}
		if (X2 > maxx) {maxx = X2;}
		if (Y1 > maxy) {maxy = Y1;}
		if (Y2 > maxy) {maxy = Y2;}
		
		/*��С�������֮��ľ���Ϊ������Ҫ��������*/
		/*���������������еĵ�*/
		/*����X����*/		
		for (i = minx; i <= maxx; i ++)
		{
			/*����Y����*/	
			for (j = miny; j <= maxy; j ++)
			{
				/*����OLED_pnpoly���ж�ָ�����Ƿ���ָ��������֮��*/
				/*����ڣ��򻭵㣬������ڣ���������*/
				if (OLED_pnpoly(3, vx, vy, i, j)) {OLED_DrawPoint(i, j);}
			}
		}
	}
}

/**
  * ��    ����OLED��Բ
  * ��    ����X ָ��Բ��Բ�ĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ��Բ��Բ�������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Radius ָ��Բ�İ뾶����Χ��0~255
  * ��    ����IsFilled ָ��Բ�Ƿ����?
  *           ��Χ��OLED_UNFILLED		�����?
  *                 OLED_FILLED			���?
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled)
{
	int16_t x, y, d, j;
	
	/* Comment removed: source encoding was damaged. */
	/*�ο��ĵ���https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf*/
	/*�ο��̳̣�https://www.bilibili.com/video/BV1VM4y1u7wJ*/
	
	d = 1 - Radius;
	x = 0;
	y = Radius;
	
	/*��ÿ���˷�֮һԲ������ʼ��*/
	OLED_DrawPoint(X + x, Y + y);
	OLED_DrawPoint(X - x, Y - y);
	OLED_DrawPoint(X + y, Y + x);
	OLED_DrawPoint(X - y, Y - x);
	
	if (IsFilled)		//ָ��Բ���?
	{
		/*������ʼ��Y����*/
		for (j = -y; j < y; j ++)
		{
			/* Comment removed: source encoding was damaged. */
			OLED_DrawPoint(X, Y + j);
		}
	}
	
	while (x < y)		//����X���ÿ����?
	{
		x ++;
		if (d < 0)		//��һ�����ڵ�ǰ�㶫��
		{
			d += 2 * x + 1;
		}
		else			//��һ�����ڵ�ǰ�㶫�Ϸ�
		{
			y --;
			d += 2 * (x - y) + 1;
		}
		
		/*��ÿ���˷�֮һԲ���ĵ�*/
		OLED_DrawPoint(X + x, Y + y);
		OLED_DrawPoint(X + y, Y + x);
		OLED_DrawPoint(X - x, Y - y);
		OLED_DrawPoint(X - y, Y - x);
		OLED_DrawPoint(X + x, Y - y);
		OLED_DrawPoint(X + y, Y - x);
		OLED_DrawPoint(X - x, Y + y);
		OLED_DrawPoint(X - y, Y + x);
		
		if (IsFilled)	//ָ��Բ���?
		{
			/*�����м䲿��*/
			for (j = -y; j < y; j ++)
			{
				/* Comment removed: source encoding was damaged. */
				OLED_DrawPoint(X + x, Y + j);
				OLED_DrawPoint(X - x, Y + j);
			}
			
			/*�������ಿ��*/
			for (j = -x; j < x; j ++)
			{
				/* Comment removed: source encoding was damaged. */
				OLED_DrawPoint(X - y, Y + j);
				OLED_DrawPoint(X + y, Y + j);
			}
		}
	}
}

/**
  * ��    ����OLED����Բ
  * ��    ����X ָ����Բ��Բ�ĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ����Բ��Բ�������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����A ָ����Բ�ĺ�����᳤�ȣ���Χ��?~255
  * ��    ����B ָ����Բ��������᳤�ȣ���Χ��?~255
  * ��    ����IsFilled ָ����Բ�Ƿ����?
  *           ��Χ��OLED_UNFILLED		�����?
  *                 OLED_FILLED			���?
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled)
{
	int16_t x, y, j;
	int16_t a = A, b = B;
	float d1, d2;
	
	/*ʹ��Bresenham�㷨����Բ�����Ա��ⲿ�ֺ�ʱ�ĸ������㣬Ч�ʸ���*/
	/*�ο����ӣ�https://blog.csdn.net/myf_666/article/details/128167392*/
	
	x = 0;
	y = b;
	d1 = b * b + a * a * (-b + 0.5);
	
	if (IsFilled)	//ָ����Բ���?
	{
		/*������ʼ��Y����*/
		for (j = -y; j < y; j ++)
		{
			/* Comment removed: source encoding was damaged. */
			OLED_DrawPoint(X, Y + j);
			OLED_DrawPoint(X, Y + j);
		}
	}
	
	/*����Բ������ʼ��*/
	OLED_DrawPoint(X + x, Y + y);
	OLED_DrawPoint(X - x, Y - y);
	OLED_DrawPoint(X - x, Y + y);
	OLED_DrawPoint(X + x, Y - y);
	
	/*����Բ�м䲿��*/
	while (b * b * (x + 1) < a * a * (y - 0.5))
	{
		if (d1 <= 0)		//��һ�����ڵ�ǰ�㶫��
		{
			d1 += b * b * (2 * x + 3);
		}
		else				//��һ�����ڵ�ǰ�㶫�Ϸ�
		{
			d1 += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
			y --;
		}
		x ++;
		
		if (IsFilled)	//ָ����Բ���?
		{
			/*�����м䲿��*/
			for (j = -y; j < y; j ++)
			{
				/* Comment removed: source encoding was damaged. */
				OLED_DrawPoint(X + x, Y + j);
				OLED_DrawPoint(X - x, Y + j);
			}
		}
		
		/*����Բ�м䲿��Բ��*/
		OLED_DrawPoint(X + x, Y + y);
		OLED_DrawPoint(X - x, Y - y);
		OLED_DrawPoint(X - x, Y + y);
		OLED_DrawPoint(X + x, Y - y);
	}
	
	/*����Բ���ಿ��*/
	d2 = b * b * (x + 0.5) * (x + 0.5) + a * a * (y - 1) * (y - 1) - a * a * b * b;
	
	while (y > 0)
	{
		if (d2 <= 0)		//��һ�����ڵ�ǰ�㶫��
		{
			d2 += b * b * (2 * x + 2) + a * a * (-2 * y + 3);
			x ++;
			
		}
		else				//��һ�����ڵ�ǰ�㶫�Ϸ�
		{
			d2 += a * a * (-2 * y + 3);
		}
		y --;
		
		if (IsFilled)	//ָ����Բ���?
		{
			/*�������ಿ��*/
			for (j = -y; j < y; j ++)
			{
				/* Comment removed: source encoding was damaged. */
				OLED_DrawPoint(X + x, Y + j);
				OLED_DrawPoint(X - x, Y + j);
			}
		}
		
		/*����Բ���ಿ��Բ��*/
		OLED_DrawPoint(X + x, Y + y);
		OLED_DrawPoint(X - x, Y - y);
		OLED_DrawPoint(X - x, Y + y);
		OLED_DrawPoint(X + x, Y - y);
	}
}

/**
  * ��    ����OLED��Բ��
  * ��    ����X ָ��Բ����Բ�ĺ����꣬��Χ��-32768~32767����Ļ����0~127
  * ��    ����Y ָ��Բ����Բ�������꣬��Χ��-32768~32767����Ļ����0~63
  * ��    ����Radius ָ��Բ���İ뾶����Χ��0~255
  * ��    ����StartAngle ָ��Բ������ʼ�Ƕȣ���Χ��-180~180
  *           ˮƽ����Ϊ0�ȣ�ˮƽ����Ϊ180�Ȼ�-180�ȣ��·�Ϊ�������Ϸ�Ϊ������˳ʱ����ת
  * ��    ����EndAngle ָ��Բ������ֹ�Ƕȣ���Χ��-180~180
  *           ˮƽ����Ϊ0�ȣ�ˮƽ����Ϊ180�Ȼ�-180�ȣ��·�Ϊ�������Ϸ�Ϊ������˳ʱ����ת
  * ��    ����IsFilled ָ��Բ���Ƿ���䣬����Ϊ����?
  *           ��Χ��OLED_UNFILLED		�����?
  *                 OLED_FILLED			���?
  * �� �� ֵ����
  * ˵    �������ô˺�����Ҫ�������س�������Ļ�ϣ�������ø��º���?
  */
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
	int16_t x, y, d, j;
	
	/*�˺�������Bresenham�㷨��Բ�ķ���*/
	
	d = 1 - Radius;
	x = 0;
	y = Radius;
	
	/*�ڻ�Բ��ÿ����ʱ���ж�ָ�����Ƿ���ָ���Ƕ��ڣ��ڣ��򻭵㣬���ڣ���������*/
	if (OLED_IsInAngle(x, y, StartAngle, EndAngle))	{OLED_DrawPoint(X + x, Y + y);}
	if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y - y);}
	if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + x);}
	if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y - x);}
	
	if (IsFilled)	//ָ��Բ�����?
	{
		/*������ʼ��Y����*/
		for (j = -y; j < y; j ++)
		{
			/* Comment removed: source encoding was damaged. */
			if (OLED_IsInAngle(0, j, StartAngle, EndAngle)) {OLED_DrawPoint(X, Y + j);}
		}
	}
	
	while (x < y)		//����X���ÿ����?
	{
		x ++;
		if (d < 0)		//��һ�����ڵ�ǰ�㶫��
		{
			d += 2 * x + 1;
		}
		else			//��һ�����ڵ�ǰ�㶫�Ϸ�
		{
			y --;
			d += 2 * (x - y) + 1;
		}
		
		/*�ڻ�Բ��ÿ����ʱ���ж�ָ�����Ƿ���ָ���Ƕ��ڣ��ڣ��򻭵㣬���ڣ���������*/
		if (OLED_IsInAngle(x, y, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y + y);}
		if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + x);}
		if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y - y);}
		if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y - x);}
		if (OLED_IsInAngle(x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y - y);}
		if (OLED_IsInAngle(y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y - x);}
		if (OLED_IsInAngle(-x, y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y + y);}
		if (OLED_IsInAngle(-y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y + x);}
		
		if (IsFilled)	//ָ��Բ�����?
		{
			/*�����м䲿��*/
			for (j = -y; j < y; j ++)
			{
				/* Comment removed: source encoding was damaged. */
				if (OLED_IsInAngle(x, j, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y + j);}
				if (OLED_IsInAngle(-x, j, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y + j);}
			}
			
			/*�������ಿ��*/
			for (j = -x; j < x; j ++)
			{
				/* Comment removed: source encoding was damaged. */
				if (OLED_IsInAngle(-y, j, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y + j);}
				if (OLED_IsInAngle(y, j, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + j);}
			}
		}
	}
}

/*********************���ܺ���*/


/*****************��Э�Ƽ�|��Ȩ����****************/
/*****************jiangxiekeji.com*****************/
