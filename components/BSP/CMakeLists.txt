set(src_dirs
            IIC
            LCD
            LED
            TOUCH
            XL9555
            QMI8658A)

set(include_dirs
            IIC
            LCD
            LED
            TOUCH
            XL9555
            QMI8658A)

set(requires
            driver
            esp_lcd)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)
