const utils = require('zigbee-herdsman-converters/lib/utils');
const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const e = require('zigbee-herdsman-converters/lib/exposes');
const ea = e.access;

const fzLocal = {
    metering_heat: {
        cluster: 'seMetering',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            if (msg.data.hasOwnProperty('currentSummDelivered')) {
                const rawValue = msg.data['currentSummDelivered'];
                const valueGcal = rawValue / 1000; // делим на 1000
                return {
                    heat_gcal: valueGcal,
                };
            }
        },
    },
};

// Функция для опроса (polling) устройства
const onEventPoll = async (type, data, device, options) => {
    const endpoint = device.getEndpoint(11); // меняем на нужный endpoint

    const poll = async () => {
        await endpoint.read('seMetering', ['currentSummDelivered']);
    };

    // Запускаем polling раз в 1 секунду
    utils.onEventPoll(type, data, device, options, 'heat_poll', 1, poll);
};

module.exports = [
    {
        zigbeeModel: ['ZigbeeMBusSensor'],
        model: 'ZigbeeMBusSensor',
        vendor: 'NAGL DIY',
        description: 'Zigbee M-Bus sensor for heat metering with polling',
        fromZigbee: [fzLocal.metering_heat],
        toZigbee: [],
        exposes: [
            {
                type: 'numeric',
                name: 'heat_gcal',
                property: 'heat_gcal',
                unit: 'Gcal',
                access: ea.STATE,
                description: 'Heat energy divided by 1000',
                device_class: 'energy',
                state_class: 'total_increasing',
            },
        ],
        onEvent: onEventPoll,
    },
];


/*const fz = require('zigbee-herdsman-converters/converters/fromZigbee');

const fzLocal = {
    metering_heat: {
        cluster: 'seMetering',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            if (msg.data.hasOwnProperty('energy')) {
                const rawValue = msg.data['energy'];
                const valueGcal = rawValue / 1000; // масштабирование
                return {
                    heat_gcal: valueGcal,
                };
            }
        },
    },
};

module.exports = [
    {
        zigbeeModel: ['ZigbeeMBusSensor'],
        model: 'ZigbeeMBusSensor',
        vendor: 'NAGL DIY',
        description: 'Zigbee M-Bus sensor for heat metering',
        fromZigbee: [fzLocal.metering_heat],
        toZigbee: [],
        exposes: [
            {
                type: 'numeric',
                name: 'heat_gcal',
                property: 'heat_gcal',
                unit: 'Gcal',
                access: 1,
                description: 'Heat energy divided by 1000',
                device_class: 'energy',
                state_class: 'total_increasing',
            },
        ],
    },
];*/


/*// zigbee2mqtt/external_converters/zigbee-mbus-sensor.js Gemini (получает Exposes)

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

const fzLocal = {
    metering_data: {
        cluster: 'seMetering',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            const attr = msg.data;

            // ИСПРАВЛЕНИЕ: Проверяем наличие атрибута по его корректному Zigbee-имени
            if (attr.hasOwnProperty('currentSummationDelivered')) {
                const buf = attr.currentSummationDelivered; // ИСПРАВЛЕНИЕ: Используем корректное имя
                if (Buffer.isBuffer(buf) && buf.length === 6) {
                    const rawValue = BigInt(buf.readUIntLE(0, 6));
                    const scaled = Number(rawValue) / 1000;
                    // Ключ в результирующем объекте оставляем таким, как определено в exposes
                    result.current_summation_delivered = parseFloat(scaled.toFixed(3));
                } else {
                    logger.warn(`Received currentSummationDelivered in unexpected format for device ${meta.device.ieeeAddr}: `, buf);
                }
            }

            return result;
        },
    },
};

const definition = {
    zigbeeModel: ['ZigbeeMBusSensor'],
    model: 'ZigbeeMBusSensor',
    vendor: 'NAGL DIY',
    description: 'DIY MBus to Zigbee meter',
    // ИСПРАВЛЕНИЕ: Вернул fz.linkquality
    fromZigbee: [fzLocal.metering_data, fz.linkquality], // Используем ваш конвертер и стандартный для LQI
    toZigbee: [],
    exposes: [
        // Оставляем кастомный numeric, который мы сделали ранее для обхода проблем со стандартным energy пресетом
        exposes.numeric('current_summation_delivered', ea.STATE)
            .withUnit('kWh')
            .withDescription('Total energy delivered')
            .withProperty('current_summation_delivered')
            .withAccess(ea.STATE)
            .withPreset('energy'), // Опционально
        e.linkquality(),
    ],
    configure: async (device, coordinatorEndpoint, logger) => { // Добавил logger
        // Хардкодная точка доступа 11, основываясь на логах.
        // Если устройство всегда использует 11 для metering, это ок.
        // Если нет, нужно вернуться к device.endpoints.find
        const endpoint = device.getEndpoint(11); 
        if (!endpoint) {
            logger.error(`Endpoint 11 not found for device ${device.ieeeAddr}`);
            return; // Выходим, если точка доступа не найдена
        }

        logger.info(`Attempting to bind seMetering cluster for ${device.ieeeAddr} on endpoint ${endpoint.ID}`);
        try {
            await endpoint.bind('seMetering', coordinatorEndpoint);
            logger.info(`Successfully bound seMetering cluster for ${device.ieeeAddr}`);
        } catch (e) {
            logger.error(`Failed to bind seMetering cluster for ${device.ieeeAddr}: ${e.message}`);
            // Пробуем продолжить настройку репортинга, даже если привязка не удалась (иногда работает)
        }


        logger.info(`Attempting to configure reporting for currentSummationDelivered (0x0000) on ${device.ieeeAddr}`);
        try {
            await endpoint.configureReporting('seMetering', [{
                attribute: 0x0000, // currentSummationDelivered
                dataType: 0x25,    // uint48 (esp_zb_uint48_t)
                minimumReportInterval: 10,
                maximumReportInterval: 300,
                reportableChange: 1,
            }]);
            logger.info(`Successfully configured reporting for currentSummationDelivered on ${device.ieeeAddr}`);
        } catch (e) {
            // ЛОГ ПОКАЗЫВАЕТ, ЧТО ЭТОТ ШАГ ЗАВЕРШАЕТСЯ ОШИБКОЙ 'UNREPORTABLE_ATTRIBUTE'
            logger.error(`Failed to configure reporting for MBus Sensor ${device.ieeeAddr}: ${e.message}`);
            // logger.error(e.stack); // Для более детальной отладки
        }
    },
};

module.exports = definition;*/

/*// zigbee2mqtt/external_converters/zigbee-mbus-sensor.js ChatGPT+Gemini

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

const fzLocal = {
    metering_data: {
        cluster: 'seMetering',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            const attr = msg.data;

            if (attr.hasOwnProperty('currentSummationDelivered')) {
                const buf = attr.currentSummationDelivered;
                if (Buffer.isBuffer(buf) && buf.length === 6) {
                    const rawValue = BigInt(buf.readUIntLE(0, 6));
                    const scaled = Number(rawValue) / 1000;
                    result.current_summation_delivered = parseFloat(scaled.toFixed(3));
                } else {
                    logger.warn(`Received currentSummationDelivered in unexpected format for device ${meta.device.ieeeAddr}: `, buf);
                }
            }

            return result;
        },
    },
};

const definition = {
    zigbeeModel: ['ZigbeeMBusSensor'],
    model: 'ZigbeeMBusSensor',
    vendor: 'NAGL DIY',
    description: 'DIY MBus to Zigbee meter',
    fromZigbee: [fzLocal.metering_data, fz.linkquality_from_basic],
    toZigbee: [],
    exposes: [
        // Оставляем кастомный numeric, который мы сделали ранее для обхода проблем со стандартным energy пресетом
        exposes.numeric('current_summation_delivered', ea.STATE)
            .withUnit('kWh')
            .withDescription('Total energy delivered')
            .withProperty('current_summation_delivered')
            .withAccess(ea.STATE)
            .withPreset('energy'), // Опционально
        e.linkquality(),
    ],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(11);
        await endpoint.bind('seMetering', coordinatorEndpoint);
        await endpoint.configureReporting('seMetering', [{
            attribute: 0x0000, // currentSummationDelivered
            dataType: 0x25,    // uint48 (esp_zb_uint48_t)
            minimumReportInterval: 10,
            maximumReportInterval: 300,
            reportableChange: 1,
        }]);
    },
};

module.exports = definition;
*/

/*// zigbee2mqtt/external_converters/zigbee-mbus-sensor.js ChatGPT version
const reporting = require('zigbee-herdsman-converters/lib/reporting');

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const {fromZigbeeConverter} = require('zigbee-herdsman-converters');
const e = exposes.presets;
const ea = exposes.access;

const fzLocal = {
    metering_data: {
        cluster: 'seMetering',
        type: ['attributeReport', 'readResponse'],
        meta: {
            multiEndpoint: false
        },        
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            const attr = msg.data;

            if (msg.data.hasOwnProperty('currentSummationDelivered'))  {
                const buf = attr.currentSummationDelivered;
                if (Buffer.isBuffer(buf) && buf.length === 6) {
                    const rawValue = BigInt(buf.readUIntLE(0, 6));
                    // Пример: делим на 1000, чтобы получить kWh
                    const scaled = Number(rawValue) / 1000;
                    result.current_summation_delivered = parseFloat(scaled.toFixed(3));
                }
            }

            return result;
        },
    },
};

const definition = {
    zigbeeModel: ['ZigbeeMBusSensor'],
    model: 'ZigbeeMBusSensor',
    vendor: 'NAGL DIY',
    description: 'DIY MBus to Zigbee meter',
    fromZigbee: [fzLocal.metering_data],
    toZigbee: [],
    exposes: [
        e.energy(), // Пресет для энергии
        e.linkquality(), // Стандартный пресет для качества связи
    ],

    configure: async (device, coordinatorEndpoint, logger) => {
        // --- ИСПРАВЛЕНО ---
        const endpoint = device.endpoints.find(ep => ep.clusters.includes('seMetering'));
        // ------------------
        if (endpoint) {
            try {
                logger.info(`Configuring reporting for MBus Sensor ${device.ieeeAddr} on endpoint ${endpoint.ID}`);
                await reporting.bind(endpoint, coordinatorEndpoint, ['seMetering']);
                // Настроить отчеты для currentSummationDelivered: мин 10с, макс 1 час, изменение на 1 (единица здесь - это 0.001 кВтч)
                await reporting.currentSummationDelivered(endpoint, {
                    min: 10,
                    max: 3600,
                    change: 1
                });
                logger.info(`Successfully configured reporting for MBus Sensor ${device.ieeeAddr}`);
            } catch (e) {
                // Логируем ошибку конфигурации более полно
                logger.error(`Failed to configure reporting for MBus Sensor ${device.ieeeAddr} on endpoint ${endpoint.ID}: ${e.message}`);
                // Выводим stack trace для более детальной диагностики, если нужно
                // logger.error(e.stack);
            }
        } else {
            logger.warn(`Could not find endpoint 11 for device ${device.ieeeAddr} during configuration.`);
        }
    },
}; 

module.exports = definition;
*/

/*// zigbee2mqtt/external_converters/zigbee-mbus-sensor.js Gemini 2.5 Pro Version

// Подключаем необходимые модули
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const utils = require('zigbee-herdsman-converters/lib/utils');
const {
    precisionRound
} = require('zigbee-herdsman-converters/lib/utils');

// Пресеты и доступ к атрибутам для exposes
const e = exposes.presets;
const ea = exposes.access;

// --- НАЧАЛО: Кастомный конвертер ---
const fz_custom_mbus_metering = {
    cluster: 'seMetering', // Работаем с кластером Metering
    type: ['attributeReport', 'readResponse'], // Реагируем на отчеты или ответы на чтение атрибутов
    convert: (model, msg, publish, options, meta) => {
        const payload = {};
        // Проверяем, пришел ли атрибут currentSummationDelivered
        if (msg.data.hasOwnProperty('currentSummationDelivered')) {
            // Получаем необработанное (масштабированное) значение.
            // herdsman обычно возвращает его как число или BigInt. Преобразуем в Number.
            const rawValue = Number(msg.data.currentSummationDelivered);

            // --- ВАЖНО: Предположение о масштабе ---
            // Ваш C++ код масштабирует значение перед отправкой.
            // Исходя из C++ кода и лога "Передано: %.3f", предполагаем, что используется 3 знака после запятой.
            // Поэтому для получения реального значения делим на 1000.0.
            const divisor = 1000.0; // Делитель для 3 знаков после запятой

            // Вычисляем реальное значение в кВт⋅ч
            const calculatedValue = rawValue / divisor;

            // Округляем до разумного числа знаков (например, 3) для чистоты
            payload.energy = precisionRound(calculatedValue, 3);

            // Лог для отладки (можно закомментировать или удалить после проверки)
            // meta.logger.debug(`MBus Sensor ${msg.device.ieeeAddr}: Received raw 'currentSummationDelivered'=${rawValue}, divided by ${divisor}, result=${payload.energy} kWh`);

        }
        // --- Добавим обработку Linkquality ---
        // Linkquality обычно добавляется самим Zigbee2MQTT, но для полноты проверим, если оно есть в meta
        if (meta && meta.linkquality) {
             payload.linkquality = meta.linkquality;
        }
        // Если нужны другие атрибуты из msg.data, обрабатываем здесь

        // Возвращаем объект с ключом 'energy' (и опционально 'linkquality')
        return payload;
    },
};
// --- КОНЕЦ: Кастомный конвертер ---

// Определение вашего устройства
const definition = {
    // ВАЖНО: Замените ['YourZigbeeModelID'] на реальный Zigbee Model ID вашего устройства.
    zigbeeModel: ['YourZigbeeModelID_REPLACE_ME'], // <-- ЗАМЕНИТЕ ЭТО!

    // Имя модели для Zigbee2MQTT
    model: 'ZigbeeMBusSensor',
    // Производитель
    vendor: 'NAGL DIY',
    // Описание
    description: 'ESP32-C6 based M-Bus Heat/Energy Meter Reader (Custom Converter)',

    // Используем НАШ кастомный конвертер
    fromZigbee: [fz_custom_mbus_metering],

    // Определяем сущности для Home Assistant
    exposes: [
        e.energy(), // Пресет для энергии
        e.linkquality(), // Стандартный пресет для качества связи
    ],

    // Настройка репортинга
    configure: async (device, coordinatorEndpoint, logger) => {
        // --- ИСПРАВЛЕНО ---
        const endpoint = device.getEndpoint(11); // Указываем номер эндпоинта напрямую (11)
        // ------------------
        if (endpoint) {
            try {
                logger.info(`Configuring reporting for MBus Sensor ${device.ieeeAddr} on endpoint ${endpoint.ID}`);
                await reporting.bind(endpoint, coordinatorEndpoint, ['seMetering']);
                // Настроить отчеты для currentSummationDelivered: мин 10с, макс 1 час, изменение на 1 (единица здесь - это 0.001 кВтч)
                await reporting.currentSummationDelivered(endpoint, {
                    min: 10,
                    max: 3600,
                    change: 1
                });
                logger.info(`Successfully configured reporting for MBus Sensor ${device.ieeeAddr}`);
            } catch (e) {
                // Логируем ошибку конфигурации более полно
                logger.error(`Failed to configure reporting for MBus Sensor ${device.ieeeAddr} on endpoint ${endpoint.ID}: ${e.message}`);
                // Выводим stack trace для более детальной диагностики, если нужно
                // logger.error(e.stack);
            }
        } else {
            logger.warn(`Could not find endpoint 11 for device ${device.ieeeAddr} during configuration.`);
        }
    },

    // Метаданные
    meta: {
        multiEndpoint: false
    },

    // Команды этому сенсору не нужны
    toZigbee: [],
};

// Экспортируем определение
module.exports = definition;
*/


/*// zigbee2mqtt/external_converters/zigbee-mbus-sensor.js Gemini 2.5 Flash Version

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const {fromZigbeeConverter} = require('zigbee-herdsman-converters');
const e = exposes.presets;
const ea = exposes.access;

const fzLocal = {
        metering_data: {
            cluster: 'seMetering',
            type: ['attributeReport', 'readResponse'],
            convert: async (model, msg, publish, options, meta) => { // Используйте async, если чтение атрибутов асинхронно
                const result = {};
                const attr = msg.data;
                const endpoint = meta.device.getEndpoint(msg.endpoint);
    
                if (attr.hasOwnProperty('currentSummationDelivered')) {
                    const buf = attr.currentSummationDelivered;
                    if (Buffer.isBuffer(buf) && buf.length === 6) {
                        const rawValue = BigInt(buf.readUIntLE(0, 6));
    
                        // --- Получаем атрибуты форматирования из Z2M ---
                        // Убедитесь, что Z2M успешно прочитал их во время интервью устройства
                        let divisor = await endpoint.getClusterAttributeValue('seMetering', 'divisor');
                        let multiplier = await endpoint.getClusterAttributeValue('seMetering', 'multiplier');
                        let summationFormatting = await endpoint.getClusterAttributeValue('seMetering', 'summationFormatting');
                           let unitOfMeasure = await endpoint.getClusterAttributeValue('seMetering', 'unitOfMeasure'); // Может пригодиться для единиц
    
                        // Если атрибуты не прочитаны Z2M, могут быть undefined или null
                        if (divisor == null || multiplier == null || summationFormatting == null) {
                            meta.logger.warn(`Metering attributes (divisor, multiplier, formatting) not available for device ${meta.device.ieeeAddr}, endpoint ${msg.endpoint}. Cannot scale currentSummationDelivered.`);
                            return {}; // Возвращаем пустой результат, т.к. невозможно масштабировать
                        }
    
    
                        // --- Применяем масштабирование с учетом summationFormatting ---
                        // Получаем количество знаков после запятой из младших 4 бит summationFormatting
                        const digitsRight = summationFormatting & 0x0F; // Младшие 4 бита
                        const scaleFactor = Math.pow(10, digitsRight);
    
                        // Делим на масштабирующий фактор и применяем multiplier/divisor
                        // Формула: (rawValue * multiplier) / (divisor * 10^digitsRight)
                        const scaled = (Number(rawValue) * Number(multiplier)) / (Number(divisor) * scaleFactor);
    
    
                        result.current_summation_delivered = parseFloat(scaled.toFixed(digitsRight)); // Форматируем до правильного кол-ва знаков
    
                        // Опционально, можно добавить единицу измерения, если Z2M ее поддерживает
                        // result.unit = 'kWh'; // Или получить из unitOfMeasure, если нужно
                    }
                }
    
                return result;
            },
        },
    };

const definition = {
    zigbeeModel: ['ZigbeeMBusSensor'],
    model: 'ZigbeeMBusSensor',
    vendor: 'NAGL DIY',
    description: 'DIY MBus to Zigbee meter',
    fromZigbee: [fzLocal.metering_data],
    toZigbee: [],
    exposes: [
        exposes.numeric('current_summation_delivered', ea.STATE_ALL) // Или ea.ALL
        .withUnit('kWh')
        .withDescription('Total heat delivered')
        .withProperty('current_summation_delivered'),
    ],
};

module.exports = definition;
*/