import { useState, useEffect, useRef } from 'react';

interface LiveBarChartProps
{
    labels: string[];
    initialData: number[];
    signalName: string;
    socket?: WebSocket;
}

const LiveBarChart: React.FC<LiveBarChartProps> = ({ labels, initialData, signalName, socket }) =>
{
    const [Chart, setChart] = useState<any>(null);
    const canvasRef = useRef<HTMLCanvasElement | null>(null);
    const chartRef = useRef<any | null>(null);
    const [data, setData] = useState<number[]>(initialData);

    useEffect(() =>
    {
        import('chart.js').then((module) =>
        {
            setChart(module.Chart);
            module.Chart.register(...module.registerables);
        });

    }, []);

    useEffect(() =>
    {
        if (!socket) return;

        const handleMessage = (event: MessageEvent) =>
        {
            console.log(event.data);
            try
            {
                const parsed = JSON.parse(event.data);
                if (parsed.signal === signalName && Array.isArray(parsed.payload))
                {
                    setData(parsed.payload);
                }
            }
            catch (e)
            {
                console.error('Invalid WebSocket message format:', e);
            }
        };

        socket.addEventListener('message', handleMessage);

        return () =>
        {
            socket.removeEventListener('message', handleMessage);
        };
    }, [socket, signalName]);

    useEffect(() =>
    {
        if (!canvasRef.current || !Chart) return;

        const ctx = canvasRef.current.getContext('2d');
        if (!ctx) return;

        if (chartRef.current)
        {
            chartRef.current.destroy();
        }

        chartRef.current = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: labels,
                datasets: [
                    {
                        label: 'Live Data',
                        data: data,
                        borderWidth: 1,
                        backgroundColor: (context: any) =>
                        {
                            const chartData = context.chart.data.datasets[0].data as number[];
                            if (!chartData.length)
                            {
                                return 'rgba(54, 162, 235, 0.6)';
                            }

                            const maxVal = Math.max(...chartData);
                            const minVal = Math.min(...chartData);

                            return chartData.map((value) =>
                            {
                                if (value === maxVal)
                                {
                                    return 'rgba(255, 0, 0, 1)';
                                }
                                const ratio = maxVal === minVal ? 0 : (value - minVal) / (maxVal - minVal);
                                const r = Math.round(255 * ratio);
                                const g = Math.round(255 * ratio);
                                const b = Math.round(200 * (1 - ratio));
                                return `rgba(${r}, ${g}, ${b}, 0.8)`;
                            });
                        },
                    },
                ],
            },
            options: {
                scales: {
                    y: { beginAtZero: true, min: 0, max: 10 },
                    x: { grid: { display: false }, ticks: { display: true } },
                },
                layout: { padding: 0 },
                elements: { bar: { borderWidth: 1 } },
                animation: {
                    duration: 10,
                    easing: 'linear',
                },
                plugins: {},
            },
        });

        return () =>
        {
            chartRef.current?.destroy();
        };
    }, [labels, data, Chart]);

    return <canvas ref={canvasRef} />;
};

export default LiveBarChart;