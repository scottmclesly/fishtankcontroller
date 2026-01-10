#ifndef CHARTS_PAGE_H
#define CHARTS_PAGE_H

const char CHARTS_PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <link rel='icon' href='data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><text y=".9em" font-size="90">🐠</text></svg>'>
    <title>Aquarium Charts</title>
    <script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js'></script>
    <script src='https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@3.0.0/dist/chartjs-adapter-date-fns.bundle.min.js'></script>
    <style>
        :root {
            --bg-primary: #0a0e1a;
            --bg-card: #1a1f2e;
            --bg-chart: #0f1419;
            --text-primary: #e0e7ff;
            --text-secondary: #94a3b8;
            --text-tertiary: #64748b;
            --color-primary: #00d4ff;
            --color-secondary: #7c3aed;
            --border-color: #1e293b;
            --shadow: rgba(0, 212, 255, 0.1);
            --glow: rgba(0, 212, 255, 0.3);
            --temp-color: #ef4444;
            --orp-color: #f59e0b;
            --ph-color: #10b981;
            --ec-color: #3b82f6;
            --tds-color: #3b82f6;
            --co2-color: #10b981;
            --nh3-color: #f59e0b;
            --nh3ppm-color: #ef4444;
            --do-color: #06b6d4;
            --stocking-color: #8b5cf6;
            --grid-color: rgba(100, 116, 139, 0.1);
        }
        [data-theme='light'] {
            --bg-primary: #f8fafc;
            --bg-card: #ffffff;
            --bg-chart: #f1f5f9;
            --text-primary: #0f172a;
            --text-secondary: #475569;
            --text-tertiary: #94a3b8;
            --color-primary: #0ea5e9;
            --color-secondary: #8b5cf6;
            --border-color: #e2e8f0;
            --shadow: rgba(14, 165, 233, 0.1);
            --glow: rgba(14, 165, 233, 0.2);
            --grid-color: rgba(100, 116, 139, 0.15);
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            padding: 20px;
            min-height: 100vh;
            transition: all 0.3s ease;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
            padding: 20px;
            background: var(--bg-card);
            border-radius: 15px;
            border: 1px solid var(--border-color);
            box-shadow: 0 4px 20px var(--shadow);
        }
        h1 {
            font-size: 2em;
            background: linear-gradient(135deg, var(--color-primary), var(--color-secondary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            font-weight: 700;
            letter-spacing: -0.5px;
        }
        .nav {
            display: flex;
            gap: 15px;
            align-items: center;
        }
        .nav a, .nav button, .theme-toggle {
            padding: 10px 20px;
            background: var(--bg-primary);
            color: var(--text-primary);
            text-decoration: none;
            border-radius: 8px;
            border: 1px solid var(--border-color);
            transition: all 0.3s ease;
            font-size: 0.9em;
            font-weight: 500;
            cursor: pointer;
        }
        .nav a:hover, .nav button:hover, .theme-toggle:hover {
            background: var(--color-primary);
            color: var(--bg-primary);
            box-shadow: 0 0 20px var(--glow);
            transform: translateY(-2px);
        }
        .stats {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-bottom: 30px;
        }
        .stat-card {
            background: var(--bg-card);
            padding: 20px;
            border-radius: 12px;
            border: 1px solid var(--border-color);
            box-shadow: 0 2px 10px var(--shadow);
            position: relative;
            overflow: hidden;
        }
        .stat-card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            width: 4px;
            height: 100%;
            background: linear-gradient(180deg, var(--stat-color), transparent);
        }
        .stat-label {
            font-size: 0.85em;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 1px;
            font-weight: 600;
        }
        .stat-value {
            font-size: 2.5em;
            font-weight: 700;
            color: var(--stat-color);
            margin: 10px 0 5px 0;
            font-variant-numeric: tabular-nums;
        }
        .stat-unit {
            font-size: 0.9em;
            color: var(--text-tertiary);
            font-weight: 500;
        }
        .stat-time {
            font-size: 0.75em;
            color: var(--text-tertiary);
            margin-top: 5px;
        }
        .chart-container {
            background: var(--bg-card);
            padding: 25px;
            border-radius: 15px;
            margin-bottom: 25px;
            border: 1px solid var(--border-color);
            box-shadow: 0 4px 20px var(--shadow);
            position: relative;
        }
        .chart-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
        }
        .chart-title {
            font-size: 1.3em;
            font-weight: 600;
            color: var(--text-primary);
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .chart-icon {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--chart-color);
            box-shadow: 0 0 10px var(--chart-color);
            animation: pulse 2s ease-in-out infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .chart-wrapper {
            position: relative;
            height: 300px;
            background: var(--bg-chart);
            border-radius: 10px;
            padding: 15px;
        }
        .status-bar {
            display: flex;
            justify-content: center;
            gap: 20px;
            padding: 15px;
            background: var(--bg-card);
            border-radius: 10px;
            margin-bottom: 20px;
            border: 1px solid var(--border-color);
            flex-wrap: wrap;
        }
        .status-item {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.85em;
            color: var(--text-secondary);
        }
        .status-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #10b981;
            animation: pulse 2s ease-in-out infinite;
        }
        .status-dot.warning { background: #f59e0b; }
        .view-toggles {
            display: flex;
            gap: 10px;
            margin-bottom: 20px;
            flex-wrap: wrap;
            justify-content: center;
        }
        .toggle-btn {
            padding: 10px 20px;
            background: var(--bg-card);
            color: var(--text-primary);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.3s ease;
            font-size: 0.9em;
            font-weight: 500;
        }
        .toggle-btn.active {
            background: var(--color-primary);
            color: var(--bg-primary);
            border-color: var(--color-primary);
            box-shadow: 0 0 15px var(--glow);
        }
        .toggle-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px var(--shadow);
        }
        .chart-section {
            transition: all 0.3s ease;
        }
        .chart-section.hidden {
            display: none;
        }
    </style>
</head>
<body>
    <div class='header'>
        <h1>📊 Kate's Aquarium #7 Analytics</h1>
        <div class='nav'>
            <button class='theme-toggle' onclick='window.location.href="/calibration"' title='Calibration'>⚙️</button>
        </div>
    </div>

    <div class='status-bar'>
        <div class='status-item'>
            <div class='status-dot' id='statusDot'></div>
            <span id='statusText'>Connecting...</span>
        </div>
        <div class='status-item'>
            <span id='mqttIndicator'>📊 MQTT: <span id='mqttStatus'>Checking...</span></span>
        </div>
        <div class='status-item'>
            <span>⏱️ Update: <span id='lastUpdate'>--</span></span>
        </div>
        <div class='status-item'>
            <span>📈 Data Points: <span id='dataPoints'>0</span></span>
        </div>
        <div class='status-item'>
            <span id='ntpStatus'>🕐 Time: Syncing...</span>
        </div>
    </div>

    <div class='stats'>
        <div class='stat-card' style='--stat-color: var(--temp-color)'>
            <div class='stat-label'>Temperature</div>
            <div class='stat-value'><span id='currentTemp'>--</span></div>
            <div class='stat-unit'>°Celsius</div>
            <div class='stat-time' id='tempTime'>--</div>
        </div>
        <div class='stat-card' style='--stat-color: var(--orp-color)'>
            <div class='stat-label'>ORP</div>
            <div class='stat-value'><span id='currentOrp'>--</span></div>
            <div class='stat-unit'>millivolts</div>
            <div class='stat-time' id='orpTime'>--</div>
        </div>
        <div class='stat-card' style='--stat-color: var(--ph-color)'>
            <div class='stat-label'>pH Level</div>
            <div class='stat-value'><span id='currentPh'>--</span></div>
            <div class='stat-unit'>pH units</div>
            <div class='stat-time' id='phTime'>--</div>
        </div>
        <div class='stat-card' style='--stat-color: var(--ec-color)'>
            <div class='stat-label'>Conductivity</div>
            <div class='stat-value'><span id='currentEc'>--</span></div>
            <div class='stat-unit'>mS/cm</div>
            <div class='stat-time' id='ecTime'>--</div>
        </div>
        <div class='stat-card' style='--stat-color: var(--tds-color)'>
            <div class='stat-label'>TDS</div>
            <div class='stat-value'><span id='currentTds'>--</span></div>
            <div class='stat-unit'>ppm</div>
            <div class='stat-time' id='tdsTime'>--</div>
        </div>
        <div class='stat-card' style='--stat-color: var(--co2-color)'>
            <div class='stat-label'>Dissolved CO₂</div>
            <div class='stat-value'><span id='currentCo2'>--</span></div>
            <div class='stat-unit'>ppm</div>
            <div class='stat-time' id='co2Time'>--</div>
        </div>
        <div class='stat-card' style='--stat-color: var(--nh3-color)'>
            <div class='stat-label'>NH₃ Ratio</div>
            <div class='stat-value'><span id='currentNh3Ratio'>--</span></div>
            <div class='stat-unit'>%</div>
            <div class='stat-time' id='nh3RatioTime'>--</div>
        </div>
        <div class='stat-card' style='--stat-color: var(--do-color)'>
            <div class='stat-label'>Max DO</div>
            <div class='stat-value'><span id='currentMaxDo'>--</span></div>
            <div class='stat-unit'>mg/L</div>
            <div class='stat-time' id='maxDoTime'>--</div>
        </div>
        <div class='stat-card' style='--stat-color: var(--stocking-color)'>
            <div class='stat-label'>Stocking Density</div>
            <div class='stat-value'><span id='currentStocking'>--</span></div>
            <div class='stat-unit'>cm/L</div>
            <div class='stat-time' id='stockingTime'>--</div>
        </div>
    </div>

    <div class='view-toggles'>
        <button class='toggle-btn active' onclick='switchView("all")' id='btnAll'>📊 All Metrics</button>
        <button class='toggle-btn' onclick='switchView("primary")' id='btnPrimary'>🔬 Primary Sensors</button>
        <button class='toggle-btn' onclick='switchView("derived")' id='btnDerived'>📈 Derived Metrics</button>
    </div>

    <div class='chart-section primary-charts'>
        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--temp-color)'></div>
                    Temperature Monitor
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='tempChart'></canvas>
            </div>
        </div>

        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--orp-color)'></div>
                    ORP (Oxidation-Reduction Potential)
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='orpChart'></canvas>
            </div>
        </div>

        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--ph-color)'></div>
                    pH Levels
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='phChart'></canvas>
            </div>
        </div>

        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--ec-color)'></div>
                    Electrical Conductivity
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='ecChart'></canvas>
            </div>
        </div>
    </div>

    <div class='chart-section derived-charts'>
        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--tds-color)'></div>
                    Total Dissolved Solids (TDS)
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='tdsChart'></canvas>
            </div>
        </div>

        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--co2-color)'></div>
                    Dissolved CO₂
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='co2Chart'></canvas>
            </div>
        </div>

        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--nh3-color)'></div>
                    Toxic Ammonia Ratio
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='nh3RatioChart'></canvas>
            </div>
        </div>

        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--do-color)'></div>
                    Maximum Dissolved Oxygen
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='maxDoChart'></canvas>
            </div>
        </div>

        <div class='chart-container'>
            <div class='chart-header'>
                <div class='chart-title'>
                    <div class='chart-icon' style='--chart-color: var(--stocking-color)'></div>
                    Stocking Density
                </div>
            </div>
            <div class='chart-wrapper'>
                <canvas id='stockingChart'></canvas>
            </div>
        </div>
    </div>

    <script>
        let charts = {};
        let historyData = [];
        let ntpSynced = false;

        function initTheme() {
            const savedTheme = localStorage.getItem('theme') || 'dark';
            document.documentElement.setAttribute('data-theme', savedTheme);
            updateThemeIcon(savedTheme);
        }

        function toggleTheme() {
            const current = document.documentElement.getAttribute('data-theme') || 'dark';
            const newTheme = current === 'light' ? 'dark' : 'light';
            document.documentElement.setAttribute('data-theme', newTheme);
            localStorage.setItem('theme', newTheme);
            updateThemeIcon(newTheme);
            updateChartTheme(newTheme);
        }

        function updateThemeIcon(theme) {
            // Theme toggle button removed from this page
            // Theme is now managed in calibration settings
        }

        function updateChartTheme(theme) {
            const isDark = theme === 'dark';
            const gridColor = isDark ? 'rgba(100, 116, 139, 0.1)' : 'rgba(100, 116, 139, 0.15)';
            const textColor = isDark ? '#94a3b8' : '#475569';

            Object.values(charts).forEach(chart => {
                chart.options.scales.x.grid.color = gridColor;
                chart.options.scales.y.grid.color = gridColor;
                chart.options.scales.x.ticks.color = textColor;
                chart.options.scales.y.ticks.color = textColor;
                chart.update('none');
            });
        }

        function createChart(canvasId, label, color, unit, suggestedMin, suggestedMax) {
            const ctx = document.getElementById(canvasId).getContext('2d');
            const isDark = (document.documentElement.getAttribute('data-theme') || 'dark') === 'dark';
            const gridColor = isDark ? 'rgba(100, 116, 139, 0.1)' : 'rgba(100, 116, 139, 0.15)';
            const textColor = isDark ? '#94a3b8' : '#475569';

            return new Chart(ctx, {
                type: 'line',
                data: {
                    datasets: [{
                        label: label,
                        data: [],
                        borderColor: color,
                        backgroundColor: color + '20',
                        borderWidth: 2,
                        tension: 0.4,
                        fill: true,
                        pointRadius: 0,
                        pointHoverRadius: 6,
                        pointHoverBackgroundColor: color,
                        pointHoverBorderColor: '#fff',
                        pointHoverBorderWidth: 2
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    interaction: {
                        intersect: false,
                        mode: 'index'
                    },
                    plugins: {
                        legend: {
                            display: false
                        },
                        tooltip: {
                            backgroundColor: 'rgba(0, 0, 0, 0.8)',
                            titleColor: '#fff',
                            bodyColor: '#fff',
                            borderColor: color,
                            borderWidth: 1,
                            padding: 12,
                            displayColors: false,
                            callbacks: {
                                label: function(context) {
                                    return label + ': ' + context.parsed.y.toFixed(2) + ' ' + unit;
                                }
                            }
                        }
                    },
                    scales: {
                        x: {
                            type: 'time',
                            time: {
                                unit: 'minute',
                                displayFormats: {
                                    minute: 'HH:mm',
                                    hour: 'HH:mm'
                                }
                            },
                            grid: {
                                color: gridColor,
                                drawBorder: false
                            },
                            ticks: {
                                color: textColor,
                                maxRotation: 0,
                                autoSkipPadding: 20
                            }
                        },
                        y: {
                            suggestedMin: suggestedMin,
                            suggestedMax: suggestedMax,
                            grid: {
                                color: gridColor,
                                drawBorder: false
                            },
                            ticks: {
                                color: textColor,
                                callback: function(value) {
                                    return value.toFixed(1) + ' ' + unit;
                                }
                            }
                        }
                    },
                    animation: {
                        duration: 750,
                        easing: 'easeInOutQuart'
                    }
                }
            });
        }

        function initCharts() {
            // Primary sensors
            charts.temp = createChart('tempChart', 'Temperature', '#ef4444', '°C', 20, 30);
            charts.orp = createChart('orpChart', 'ORP', '#f59e0b', 'mV', 200, 400);
            charts.ph = createChart('phChart', 'pH', '#10b981', 'pH', 6, 9);
            charts.ec = createChart('ecChart', 'EC', '#3b82f6', 'mS/cm', 0, 5);

            // Derived metrics
            charts.tds = createChart('tdsChart', 'TDS', '#3b82f6', 'ppm', 0, 500);
            charts.co2 = createChart('co2Chart', 'CO₂', '#10b981', 'ppm', 0, 40);
            charts.nh3Ratio = createChart('nh3RatioChart', 'NH₃ Ratio', '#f59e0b', '%', 0, 10);
            charts.maxDo = createChart('maxDoChart', 'Max DO', '#06b6d4', 'mg/L', 6, 12);
            charts.stocking = createChart('stockingChart', 'Stocking', '#8b5cf6', 'cm/L', 0, 3);
        }

        function updateCharts(data) {
            if (!data || data.length === 0) return;

            const timestamps = data.map(d => new Date(d.t * 1000));

            // Primary sensors
            charts.temp.data.labels = timestamps;
            charts.temp.data.datasets[0].data = data.map(d => parseFloat(d.temp));
            charts.temp.update('none');

            charts.orp.data.labels = timestamps;
            charts.orp.data.datasets[0].data = data.map(d => parseFloat(d.orp));
            charts.orp.update('none');

            charts.ph.data.labels = timestamps;
            charts.ph.data.datasets[0].data = data.map(d => parseFloat(d.ph));
            charts.ph.update('none');

            charts.ec.data.labels = timestamps;
            charts.ec.data.datasets[0].data = data.map(d => parseFloat(d.ec));
            charts.ec.update('none');

            // Derived metrics
            if (data[0].tds !== undefined) {
                charts.tds.data.labels = timestamps;
                charts.tds.data.datasets[0].data = data.map(d => parseFloat(d.tds || 0));
                charts.tds.update('none');

                charts.co2.data.labels = timestamps;
                charts.co2.data.datasets[0].data = data.map(d => parseFloat(d.co2 || 0));
                charts.co2.update('none');

                charts.nh3Ratio.data.labels = timestamps;
                charts.nh3Ratio.data.datasets[0].data = data.map(d => (parseFloat(d.nh3_ratio || 0) * 100));
                charts.nh3Ratio.update('none');

                charts.maxDo.data.labels = timestamps;
                charts.maxDo.data.datasets[0].data = data.map(d => parseFloat(d.max_do || 0));
                charts.maxDo.update('none');

                charts.stocking.data.labels = timestamps;
                charts.stocking.data.datasets[0].data = data.map(d => parseFloat(d.stocking || 0));
                charts.stocking.update('none');
            }
        }

        async function fetchHistory() {
            try {
                const response = await fetch('/api/history');
                const json = await response.json();

                historyData = json.data || [];
                ntpSynced = json.ntp_synced;

                document.getElementById('dataPoints').textContent = json.count || 0;
                document.getElementById('ntpStatus').textContent = ntpSynced
                    ? '🕐 Time: Synced'
                    : '🕐 Time: Not Synced';

                if (ntpSynced) {
                    document.getElementById('ntpStatus').style.color = '#10b981';
                }

                updateCharts(historyData);

                document.getElementById('statusDot').classList.remove('warning');
                document.getElementById('statusText').textContent = 'Connected';
            } catch (error) {
                console.error('Error fetching history:', error);
                document.getElementById('statusDot').classList.add('warning');
                document.getElementById('statusText').textContent = 'Connection Error';
            }
        }

        async function fetchCurrentData() {
            try {
                const response = await fetch('/api/sensors');
                const data = await response.json();

                if (data.valid) {
                    // Primary sensors
                    document.getElementById('currentTemp').textContent = data.temperature_c.toFixed(2);
                    document.getElementById('currentOrp').textContent = data.orp_mv.toFixed(2);
                    document.getElementById('currentPh').textContent = data.ph.toFixed(2);
                    document.getElementById('currentEc').textContent = data.ec_ms_cm.toFixed(3);

                    const now = new Date().toLocaleTimeString();
                    document.getElementById('tempTime').textContent = now;
                    document.getElementById('orpTime').textContent = now;
                    document.getElementById('phTime').textContent = now;
                    document.getElementById('ecTime').textContent = now;

                    // Fetch derived metrics
                    fetch('/api/metrics/derived').then(r => r.json()).then(derived => {
                        if (derived) {
                            document.getElementById('currentTds').textContent = parseFloat(derived.tds_ppm).toFixed(1);
                            document.getElementById('currentCo2').textContent = parseFloat(derived.co2_ppm).toFixed(2);
                            document.getElementById('currentNh3Ratio').textContent = (parseFloat(derived.toxic_ammonia_ratio) * 100).toFixed(2);
                            document.getElementById('currentMaxDo').textContent = parseFloat(derived.max_do_mg_l).toFixed(2);
                            document.getElementById('currentStocking').textContent = parseFloat(derived.stocking_density).toFixed(2);

                            document.getElementById('tdsTime').textContent = now;
                            document.getElementById('co2Time').textContent = now;
                            document.getElementById('nh3RatioTime').textContent = now;
                            document.getElementById('maxDoTime').textContent = now;
                            document.getElementById('stockingTime').textContent = now;
                        }
                    }).catch(err => console.log('Derived metrics not available:', err));
                }
            } catch (error) {
                console.error('Error fetching current data:', error);
            } finally {
                document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
            }
        }

        function switchView(view) {
            // Update button states
            document.getElementById('btnAll').classList.remove('active');
            document.getElementById('btnPrimary').classList.remove('active');
            document.getElementById('btnDerived').classList.remove('active');

            const primaryCharts = document.querySelector('.primary-charts');
            const derivedCharts = document.querySelector('.derived-charts');

            if (view === 'all') {
                document.getElementById('btnAll').classList.add('active');
                primaryCharts.classList.remove('hidden');
                derivedCharts.classList.remove('hidden');
            } else if (view === 'primary') {
                document.getElementById('btnPrimary').classList.add('active');
                primaryCharts.classList.remove('hidden');
                derivedCharts.classList.add('hidden');
            } else if (view === 'derived') {
                document.getElementById('btnDerived').classList.add('active');
                primaryCharts.classList.add('hidden');
                derivedCharts.classList.remove('hidden');
            }
        }

        async function exportCSV() {
            try {
                const response = await fetch('/api/export/csv');
                const blob = await response.blob();

                const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
                const filename = `aquarium-data-${timestamp}.csv`;

                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
            } catch (error) {
                console.error('CSV export failed:', error);
                alert('Failed to export CSV. Please try again.');
            }
        }

        async function exportJSON() {
            try {
                const response = await fetch('/api/export/json');
                const blob = await response.blob();

                const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
                const filename = `aquarium-data-${timestamp}.json`;

                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
            } catch (error) {
                console.error('JSON export failed:', error);
                alert('Failed to export JSON. Please try again.');
            }
        }

        function updateMqttStatus() {
            fetch('/api/mqtt/status')
                .then(response => response.json())
                .then(data => {
                    const statusEl = document.getElementById('mqttStatus');
                    if (data.connected) {
                        statusEl.textContent = '✓ Connected';
                        statusEl.style.color = '#10b981';
                    } else if (data.enabled) {
                        statusEl.textContent = '⚠ ' + data.status;
                        statusEl.style.color = '#f59e0b';
                    } else {
                        statusEl.textContent = 'Disabled';
                        statusEl.style.color = '#64748b';
                    }
                })
                .catch(err => {
                    console.error('MQTT status fetch failed:', err);
                });
        }

        initTheme();
        initCharts();
        fetchHistory();
        fetchCurrentData();
        updateMqttStatus();

        setInterval(fetchHistory, 5000);
        setInterval(fetchCurrentData, 2000);
        setInterval(updateMqttStatus, 5000);
    </script>

    <div style='text-align: center; padding: 20px; color: var(--text-secondary); font-size: 0.85em; background: var(--bg-card); border-radius: 10px; margin-top: 20px; border: 1px solid var(--border-color);'>
        Scott McLelslie to my beloved wife Kate 2026. Happy new year!
    </div>
</body>
</html>
)rawliteral";

#endif // CHARTS_PAGE_H
