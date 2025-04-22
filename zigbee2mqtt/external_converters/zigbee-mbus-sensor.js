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
        await endpoint.read('genPowerCfg', ['batteryPercentageRemaining']);
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
        fromZigbee: [fzLocal.metering_heat, fz.battery],
        toZigbee: [],
        exposes: [
            {
                type: 'numeric',
                name: 'heat_gcal',
                property: 'heat_gcal',
                unit: 'Gcal',
                access: ea.STATE,
                description: 'Heat energy divided',
                device_class: 'energy',
                state_class: 'total_increasing',
            },
            {
                type: 'numeric',
                name: 'battery',
                property: 'battery',
                unit: '%',
                access: ea.STATE,
                description: 'Battery percentage',
                device_class: 'battery',
                state_class: 'measurement',
            },
        ],
        onEvent: onEventPoll,
    },
];
