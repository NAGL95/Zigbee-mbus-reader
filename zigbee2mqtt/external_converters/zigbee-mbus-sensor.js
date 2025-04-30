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
                if (rawValue === 0) return; // Пропускаем 0
                const valueGcal = rawValue / 1000;
                return { heat_gcal: valueGcal };
            }
        },
    },
    power_voltage: {
        cluster: 'genPowerCfg',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            if (!msg.data.hasOwnProperty('mainsVoltage')) return;
    
            const rawValue = msg.data['mainsVoltage'];
            if (rawValue === 0) return;
    
            const voltage = rawValue / 100.0; // mainsVoltage приходит как В * 100
    
            // Таблица соответствия напряжения уровню заряда
            let batteryPercent;
            if (voltage >= 4.20) batteryPercent = 100;
            else if (voltage >= 4.00) batteryPercent = 85;
            else if (voltage >= 3.80) batteryPercent = 60;
            else if (voltage >= 3.70) batteryPercent = 45;
            else if (voltage >= 3.60) batteryPercent = 30;
            else if (voltage >= 3.50) batteryPercent = 15;
            else if (voltage >= 3.30) batteryPercent = 5;
            else batteryPercent = 0;
    
            return {
                mains_voltage: voltage,
                battery: batteryPercent,
            };
        },
    },
    
};

const onEventPoll = async (type, data, device, options) => {
    if (type === 'stop') return;

    const endpoint = device.getEndpoint(11);

    const poll = async () => {
        await endpoint.read('seMetering', ['currentSummDelivered']);
        await endpoint.read('genPowerCfg', ['batteryPercentageRemaining', 'mainsVoltage']);
    };

    utils.onEventPoll(type, data, device, options, 'heat_poll', 1, poll);
};

module.exports = [
    {
        zigbeeModel: ['ZigbeeMBusSensor'],
        model: 'ZigbeeMBusSensor',
        vendor: 'NAGL DIY',
        description: 'Zigbee M-Bus sensor for heat metering with polling',
        fromZigbee: [fzLocal.metering_heat, fz.battery, fzLocal.power_voltage],
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
            {
                type: 'numeric',
                name: 'mains_voltage',
                property: 'mains_voltage',
                unit: 'V',
                access: ea.STATE,
                description: 'Mains voltage',
                device_class: 'voltage',
                state_class: 'measurement',
            },
        ],
        onEvent: onEventPoll,
    },
];
