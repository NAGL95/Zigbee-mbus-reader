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
    
            const voltage = rawValue / 100.0;
    
            // Прямая интерполяция от 3.0V (0%) до 4.2V (100%)
            const minV = 3.0;
            const maxV = 4.2;
            let percent = ((voltage - minV) / (maxV - minV)) * 100;
    
            // Обрезаем диапазон от 0 до 100
            percent = Math.max(0, Math.min(100, percent));
            const batteryPercent = Math.round(percent); // Округляем до целого
    
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
