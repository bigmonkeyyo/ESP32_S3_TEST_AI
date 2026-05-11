#include "iic.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

i2c_obj_t iic_master[I2C_NUM_MAX];
const char *iic_name = "iic.c";

i2c_obj_t iic_init(uint8_t iic_port)
{
    uint8_t index = (iic_port == I2C_NUM_0) ? 0 : 1;
    i2c_config_t iic_config_struct = {0};

    iic_master[index].port = iic_port;
    iic_master[index].init_flag = ESP_FAIL;

    if (iic_master[index].port == I2C_NUM_0) {
        iic_master[index].scl = IIC0_SCL_GPIO_PIN;
        iic_master[index].sda = IIC0_SDA_GPIO_PIN;
    } else {
        iic_master[index].scl = IIC1_SCL_GPIO_PIN;
        iic_master[index].sda = IIC1_SDA_GPIO_PIN;
    }

    iic_config_struct.mode = I2C_MODE_MASTER;
    iic_config_struct.sda_io_num = iic_master[index].sda;
    iic_config_struct.scl_io_num = iic_master[index].scl;
    iic_config_struct.sda_pullup_en = GPIO_PULLUP_ENABLE;
    iic_config_struct.scl_pullup_en = GPIO_PULLUP_ENABLE;
    iic_config_struct.master.clk_speed = IIC_FREQ;

    ESP_ERROR_CHECK(i2c_param_config(iic_master[index].port, &iic_config_struct));
    iic_master[index].init_flag = i2c_driver_install(iic_master[index].port,
                                                     iic_config_struct.mode,
                                                     I2C_MASTER_RX_BUF_DISABLE,
                                                     I2C_MASTER_TX_BUF_DISABLE,
                                                     0);

    if (iic_master[index].init_flag != ESP_OK) {
        while (1) {
            ESP_LOGI(iic_name, "%s, ret: %d", __func__, iic_master[index].init_flag);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    return iic_master[index];
}

esp_err_t i2c_transfer(i2c_obj_t *self, uint16_t addr, size_t n, i2c_buf_t *bufs, unsigned int flags)
{
    int data_len = 0;
    esp_err_t ret = ESP_FAIL;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    if ((self == NULL) || (bufs == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cmd == NULL) {
        ESP_LOGW(iic_name, "i2c command link create failed");
        return ESP_ERR_NO_MEM;
    }

    if (flags & I2C_FLAG_WRITE) {
        ret = i2c_master_start(cmd);
        if (ret != ESP_OK) {
            goto cleanup;
        }

        ret = i2c_master_write_byte(cmd, addr << 1, ACK_CHECK_EN);
        if (ret != ESP_OK) {
            goto cleanup;
        }

        ret = i2c_master_write(cmd, bufs->buf, bufs->len, ACK_CHECK_EN);
        if (ret != ESP_OK) {
            goto cleanup;
        }

        data_len += bufs->len;
        --n;
        ++bufs;
    }

    ret = i2c_master_start(cmd);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    ret = i2c_master_write_byte(cmd, addr << 1 | (flags & I2C_FLAG_READ), ACK_CHECK_EN);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    for (; n--; ++bufs) {
        if (flags & I2C_FLAG_READ) {
            ret = i2c_master_read(cmd, bufs->buf, bufs->len,
                                  n == 0 ? I2C_MASTER_LAST_NACK : I2C_MASTER_ACK);
        } else if (bufs->len != 0) {
            ret = i2c_master_write(cmd, bufs->buf, bufs->len, ACK_CHECK_EN);
        } else {
            ret = ESP_OK;
        }

        if (ret != ESP_OK) {
            goto cleanup;
        }
        data_len += bufs->len;
    }

    if (flags & I2C_FLAG_STOP) {
        ret = i2c_master_stop(cmd);
        if (ret != ESP_OK) {
            goto cleanup;
        }
    }

    ret = i2c_master_cmd_begin(self->port, cmd, 100 * (1 + data_len) / portTICK_PERIOD_MS);

cleanup:
    if (ret != ESP_OK) {
        ESP_LOGW(iic_name, "i2c transfer failed: %s", esp_err_to_name(ret));
    }
    i2c_cmd_link_delete(cmd);
    return ret;
}
