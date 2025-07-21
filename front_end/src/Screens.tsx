
import LiveBarChart from './components/LiveBarChart';
import MirroredVerticalBarChart from './components/MirroredVerticalBarChart';
import StreamingScatterPlot from './components/StreamingScatterPlot';
import DualSignalPlot from './components/DualSignalPlot';
import ScrollingHeatmap from './components/ScrollingHeatMap';
import { RenderTickProvider } from './components/RenderingTick';
import LedRow from './components/LedRow';
import SignalValueTextBox from './components/SignalValueTextbox';
import HorizontalGauge from './components/HorizontalGauge';
import { WebSocketContextType } from './components/WebSocketContext';
import Incrementer from './components/Incrementer';
import ValueSelector from './components/ValueSelector';
import ltop from './assets/ltop.png';
export type ScreenType = (typeof SCREENS)[keyof typeof SCREENS];

interface ScreenProps {
    socket: WebSocketContextType;
}

interface RenderScreenParams {
    socket: WebSocketContextType;
    screen: string;
}

export const SCREENS = {
  HOME: 'home',
  TOWER_SCREEN: 'tower screen',
  HORIZONTAL_STEREO_SPECTRUM: 'horizontal stereo spectrum',
  VERTICAL_STEREO_SPECTRUM: 'vertical stereo spectrum',
  WAVE_SCREEN: 'wave screen',
  SCROLLING_HEAT_MAP: 'scrolling heat map screen',
  SCROLLING_HEAT_MAP_RAINBOW: 'scrolling heat map rainbow',
  SETTING_SENSITIVITY: 'setting sensitivity',
  SETTING_CURRENT_LIMIT: 'setting current limit',
  SETTING_RENDERING: 'setting rendering',
  SETTING_FREQUENCY_RENDERING: 'setting frequency rendering',
  PERFORMANCE_SCREEN: 'performance screen',
} as const;

export const renderScreen = ({ socket, screen }: RenderScreenParams) => {
    switch (screen) {
        case SCREENS.HOME:
          return <HomeScreen />;
        case SCREENS.TOWER_SCREEN:
          return <TowerScreen socket={socket} />;
        case SCREENS.HORIZONTAL_STEREO_SPECTRUM:
          return <HorizontalStereoSpectrumScreen socket={socket} />;
        case SCREENS.VERTICAL_STEREO_SPECTRUM:
          return <VerticalStereoSpectrumScreen socket={socket} />;
        case SCREENS.WAVE_SCREEN:
          return <WaveScreen socket={socket}  />;
        case SCREENS.SCROLLING_HEAT_MAP_RAINBOW:
          return <ScrollingHeatMapRainbowScreen socket={socket} />;
        case SCREENS.SCROLLING_HEAT_MAP:
          return <ScrollingHeatMapScreen socket={socket} />;
        case SCREENS.SETTING_SENSITIVITY:
          return <SettingBrightnessScreen socket={socket} />;
        case SCREENS.SETTING_CURRENT_LIMIT:
          return <SettingCurrentLimitScreen socket={socket} />;
        case SCREENS.SETTING_RENDERING:
          return <SettingRenderingScreen socket={socket} />;
        case SCREENS.SETTING_FREQUENCY_RENDERING:
          return <SettingFrequencyRenderingScreen socket={socket} />;
        case SCREENS.PERFORMANCE_SCREEN:
          return <PerformanceScreen socket={socket} />;
        default:
        return <HomeScreen />;
    }
};

export function HomeScreen() {
  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'center',
        alignItems: 'center',
        backgroundColor: '#111',
        color: 'white',
        padding: '20px',
        boxSizing: 'border-box'
      }}
    >
      <h1 style={{ fontSize: '3em', margin: 0, textAlign: 'center' }}>LED Tower of Power</h1>

      <div
        style={{
          width: '100%',
          maxWidth: '600px',
          height: '300px',
          marginTop: '20px',
          alignItems: 'center',
        }}
      >
        <img
          src={ltop}
          alt="LTOP"
          style={{
            width: '100%',
            height: '100%',
            objectFit: 'contain'
          }}
        />
      </div>

      <p style={{ fontSize: '1.5em', marginTop: '20px', textAlign: 'center' }}>
        Select a screen from the menu to get started.
      </p>
    </div>
  );
}

export function TowerScreen({ socket }: ScreenProps) {
    const gridStyle = {
        width: '100%',
        height:'100%',
        display: 'grid',
        gridTemplateColumns: 'auto',
        gridTemplateRows: 'auto',
        rowGap: '0px',
        columnGap: '0px',
        backgroundColor: 'black',
        padding: '0px',
    };

    const rowStyle = {
      backgroundColor: 'darkgray',
    };
  
    /*const textStyle = {
      fontSize: 'clamp(14px, 5vw, 24px)',
      padding: '2px',
    };*/
    
    return (
      <div style={gridStyle}>
        {Array.from({ length: 144 }, (_, i) => (
          <>
            <div key={`led-${i}`} style={rowStyle}>
              <LedRow ledCount={5} signal="Pixel Grid" rowIndex={i} socket={socket} randomMode={false} />
            </div>
          </>
        ))}
      </div>
    );
  }

  export function PerformanceScreen({ socket }: ScreenProps) {
    const itemStyle = {
      backgroundColor: 'transparent',
      justifyContent: 'flex-end',
      textAlign: 'right'as const,
      height:'50px',
      display: 'flex', 
      alignItems: 'center',
      fontSize: '20px',
      textShadow: `
          -1px -1px 0 #000,
            1px -1px 0 #000,
          -1px  1px 0 #000,
            1px  1px 0 #000
      `,
    };
    const iconstyle={
      ...itemStyle,
      justifyContent: 'center',
      lineHeight: '1',
      fontSize: '40px',
    };
    return (
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: '1fr auto 4fr',
          gap: '8px',
          alignItems: 'center', // general vertical centering
        }}
      >
        {/* CPU Usage */}
        <div style={itemStyle}>
          CPU Usage
        </div>
        <div
          style={iconstyle}
        >
          🖥️
        </div>
        <div style={itemStyle}>
          <HorizontalGauge
            min={0}
            max={100}
            signal={"CPU Usage"}
            socket={socket}
            zones={[{ from: 0, to: 100, color: 'green' }]}
            tickMarks={[0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]}
            tickMarkLabels={['0%', '10%', '20%', '30%', '40%', '50%', '60%', '70%', '80%', '90%', '100%']}
          />
        </div>

        {/* Memory Usage */}
        <div style={itemStyle}>
          Memory Usage
        </div>
        <div
          style={iconstyle}
        >
          💾
        </div>
        <div style={itemStyle}>
          <HorizontalGauge
            min={0}
            max={100}
            signal={"CPU Memory Usage"}
            socket={socket}
            zones={[{ from: 0, to: 100, color: 'green' }]}
            tickMarks={[0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]}
            tickMarkLabels={['0%', '10%', '20%', '30%', '40%', '50%', '60%', '70%', '80%', '90%', '100%']}
          />
        </div>

        {/* CPU Temp */}
        <div style={itemStyle}>
          CPU Temp
        </div>
        <div
          style={iconstyle}
        >
          🌡️
        </div>
        <div style={itemStyle}>
          <HorizontalGauge
            min={0}
            max={100}
            signal={"CPU Temp"}
            socket={socket}
            zones={[
              { from: 0, to: 70, color: 'green' },
              { from: 70, to: 80, color: 'yellow' },
              { from: 80, to: 100, color: 'red' },
            ]}
            tickMarks={[0, 20, 40, 60, 80, 100]}
            tickMarkLabels={['0 °C', '20 °C', '40 °C', '60 °C', '80 °C', '100 °C']}
          />
        </div>

        {/* Left Mic Db */}
        <div style={itemStyle}>
          Left Mic Db
        </div>
        <div
          style={iconstyle}
        >
          🎤
        </div>
        <div style={itemStyle}>
          <HorizontalGauge
            min={0}
            max={120}
            signal={"FFT Computer Left Channel Power SPL"}
            socket={socket}
            zones={[
              { from: 0, to: 70, color: 'green' },
              { from: 70, to: 90, color: 'yellow' },
              { from: 90, to: 120, color: 'red' },
            ]}
            tickMarks={[0, 20, 40, 60, 80, 100, 120]}
            tickMarkLabels={['0 Db','20 Db', '40 Db', '60 Db', '80 Db', '100 Db', '120 Db']}
          />
        </div>

        {/* Left Mic % */}
        <div style={itemStyle}>
          Left Mic %
        </div>
        <div
          style={iconstyle}
        >
          🎤
        </div>
        <div style={itemStyle}>
          <HorizontalGauge
            min={0}
            max={1}
            signal={"FFT Computer Right Channel Power Normalized"}
            socket={socket}
            zones={[
              { from: 0.0, to: 1.0, color: 'green' },
            ]}
            tickMarks={[0.0, 0.20, 0.40, 0.60, 0.80, 1.0]}
            tickMarkLabels={['0 %','20 %', '40 %', '60 %', '80 %', '100 %']}
          />
        </div>
        
        {/* Right Mic Db */}
        <div style={itemStyle}>
          Right Mic Db
        </div>
        <div
          style={iconstyle}
        >
          🎤
        </div>
        <div style={itemStyle}>
          <HorizontalGauge
            min={0}
            max={120}
            signal={"FFT Computer Left Channel Power SPL"}
            socket={socket}
            zones={[
              { from: 0, to: 70, color: 'green' },
              { from: 70, to: 90, color: 'yellow' },
              { from: 90, to: 120, color: 'red' },
            ]}
            tickMarks={[0, 20, 40, 60, 80, 100, 120]}
            tickMarkLabels={['0 Db','20 Db', '40 Db', '60 Db', '80 Db', '100 Db', '120 Db']}
          />
        </div>
        
        {/* Right Mic % */}
        <div style={itemStyle}>
          Right Mic %
        </div>
        <div
          style={iconstyle}
        >
          🎤
        </div>
        <div style={itemStyle}>
          <HorizontalGauge
            min={0}
            max={1}
            signal={"FFT Computer Left Channel Power Normalized"}
            socket={socket}
            zones={[
              { from: 0.0, to: 1.0, color: 'green' },
            ]}
            tickMarks={[0.0, 0.20, 0.40, 0.60, 0.80, 1.0]}
            tickMarkLabels={['0 %','20 %', '40 %', '60 %', '80 %', '100 %']}
          />
        </div>
      </div>

    );
  }

  export function HorizontalStereoSpectrumScreen({ socket }: ScreenProps) {
    return (
      <div style={{ display: 'flex', width: '100%', height: '100%' }}>
        <div style={{ width: '50%', height: '100%' }}>
          <LiveBarChart
            signal="FFT Computer Left Channel FFT Normalized"
            yLabelPosition="left"
            barColor="rgba(54, 162, 235, 0.6)"
            xLabelMinRotation={90}
            xLabelMaxRotation={90}
            flipX={true}
            socket={socket}
          />
        </div>
        <div style={{ width: '50%', height: '100%' }}>
          <LiveBarChart
            signal="FFT Computer Right Channel FFT Normalized"
            yLabelPosition="right"
            barColor="rgba(255, 99, 132, 0.6)"
            xLabelMinRotation={90}
            xLabelMaxRotation={90}
            flipX={false}
            socket={socket}
          />
        </div>
      </div>
    );
  }

  export function VerticalStereoSpectrumScreen({ socket }: ScreenProps) {
    return (
      <div style={{ width: '100%', height: '100%' }}>
        <MirroredVerticalBarChart
          leftSignal="FFT Computer Left Channel FFT Normalized"
          rightSignal="FFT Computer Right Channel FFT Normalized"
          socket={socket}
        />
      </div>
    );
  }

  export function WaveScreen({ socket }: ScreenProps) {
    return (
      <div style={{ width: '100%', height: '100%' }}>
        <StreamingScatterPlot 
            signal1="Microphone Right Channel"
            signal2="Microphone Left Channel"
            horizontalMinSignal="Min Microphone Limit"
            horizontalMaxSignal="Max Microphone Limit"
            socket={socket} />
      </div>
    );
  }

export function ScrollingHeatMapRainbowScreen({ socket }: ScreenProps) {
    return (
      <RenderTickProvider>
        <div style={{ display: 'flex', width: '100%', height: '100%' }}>
          <div style={{ width: '50%', height: '100%' }}>
            <ScrollingHeatmap
              signal="FFT Computer Left Channel FFT Normalized"
              dataWidth={32}
              dataHeight={1000}
              min={0}
              max={1}
              flipX={true}
              mode={'Rainbow'}
              socket={socket}
            />
          </div>
          <div style={{ width: '50%', height: '100%' }}>
            <ScrollingHeatmap
              signal="FFT Computer Right Channel FFT Normalized"
              dataWidth={32}
              dataHeight={1000}
              min={0}
              max={1}
              flipX={false}
              mode={'Rainbow'}
              socket={socket}
            />
          </div>
        </div>
      </RenderTickProvider>
    );
}

export function ScrollingHeatMapScreen({ socket }: ScreenProps) {
    return (
      <RenderTickProvider>
        <div style={{ display: 'flex', width: '100%', height: '100%' }}>
          <div style={{ width: '50%', height: '100%' }}>
            <ScrollingHeatmap
              signal="FFT Computer Left Channel FFT Normalized"
              dataWidth={32}
              dataHeight={1000}
              min={0}
              max={1}
              flipX={true}
              minColor={'#000000'}
              midColor={'#0000ff'}
              maxColor={'#ffff00'}
              socket={socket}
            />
          </div>
          <div style={{ width: '50%', height: '100%' }}>
            <ScrollingHeatmap
              signal="FFT Computer Right Channel FFT Normalized"
              dataWidth={32}
              dataHeight={1000}
              min={0}
              max={1}
              flipX={false}
              minColor={'#000000'}
              midColor={'#ff0000'}
              maxColor={'#ffff00'}
              socket={socket}
            />
          </div>
        </div>
      </RenderTickProvider>
    );
}

export function SettingRenderingScreen({ socket }: ScreenProps) {
  const gridStyle = {
      width: '100%',
      height:'100%',
      display: 'grid',
      gridTemplateColumns: 'auto',
      gridTemplateRows: 'auto',
      rowGap: '0px',
      columnGap: '0px',
      backgroundColor: 'black',
      padding: '0px',
  };
  const itemStyle = {
    backgroundColor: 'darkgray',
  };
  return (
        <div style={{
        display: 'flex', 
        height: '100%', 
        width: '100%', 
        justifyContent: 'center', 
        alignItems: 'center', 
        backgroundColor: '#111', 
        color: 'white', 
        flexDirection: 'column', 
        gap: 20, 
        padding: 20, 
        boxSizing: 'border-box',
        overflowY: 'auto',
    }}>
      
      {/* Row container to keep things consistently aligned */}
      <div style={{ width: '100%', display: 'flex', flexDirection: 'column', height: '100%' }}>
        <div style={{...gridStyle, flex: '1', minHeight:0, overflowY: 'auto',}}>
          {Array.from({ length: 144 }, (_, i) => (
            <>
              <div key={`led-${i}`} style={itemStyle}>
                <LedRow ledCount={5} signal="Pixel Grid" rowIndex={i} socket={socket} randomMode={false} />
              </div>
            </>
          ))}
        </div>
        {/* Row for Color Mapping */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'row', alignItems: 'center', gap: 10, justifyContent: 'center' }}>
          <div style={{ width: 200, textAlign: 'right' }}>
            <h2 style={{ margin: 0, userSelect: 'none' }}>Color Mapping</h2>
          </div>
          <ValueSelector
            signal="Color Mapping Type"
            socket={socket}
            options={['Linear', 'Log2', 'Log10']}
            label="Color Mapping Type"
            onChange={(val) => console.log('Selected:', val)}
          />
        </div>
      </div>
    </div>
  );
}

export function SettingBrightnessScreen({ socket }: ScreenProps) {
  const containerStyle: React.CSSProperties = {
    display: 'flex',
    flexDirection: 'column',
    minHeight: '100%',
    width: '100%',
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: 'black',
    color: 'white',
    padding: 20,
    boxSizing: 'border-box',
    overflowY: 'auto',
    gap: 20,
  };

  const sectionStyle: React.CSSProperties = {
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    gap: 20,
  };

  const rowStyle: React.CSSProperties = {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    gap: 12,
    flexWrap: 'wrap',
  };

  const labelStyle: React.CSSProperties = {
    width: 200,
    textAlign: 'right',
    fontSize: 16,
    fontWeight: 'bold',
    userSelect: 'none',
    whiteSpace: 'nowrap',
  };

  const chartRowStyle: React.CSSProperties = {
    display: 'flex',
    flexDirection: 'row',
    justifyContent: 'center',
    gap: 10,
    height: '20vh',
    width: '100%',
  };

  const cellStyle: React.CSSProperties = {
    border: '1px solid #444',
    padding: '8px 12px',
    textAlign: 'left',
  };

  const tableStyle: React.CSSProperties = {
    borderCollapse: 'collapse',
    width: '100%',
    backgroundColor: '#222',
    color: 'white',
    fontSize: 14,
    marginTop: 20,
  };

  const column4gridStyle: React.CSSProperties = { 
      display: 'grid',
      gridTemplateColumns: '1fr 1fr 4fr 4fr',
      gridTemplateRows: 'auto auto auto auto',
      gap: '10px',
  };

  const itemStyle = {
      backgroundColor: 'transparent',
      justifyContent: 'flex-end',
      textAlign: 'right'as const,
      height:'50px',
      display: 'flex', 
      alignItems: 'center',
      fontSize: '20px',
      textShadow: `
          -1px -1px 0 #000,
            1px -1px 0 #000,
          -1px  1px 0 #000,
            1px  1px 0 #000
      `,
    };
    const iconstyle={
      ...itemStyle,
      justifyContent: 'center',
      lineHeight: '1',
      fontSize: '40px',
    };

  return (
    <div style={containerStyle}>
      <div style={sectionStyle}>
        {/* FFT Graphs */}
        <div style={chartRowStyle}>
          <div style={{ width: '50%', height: '100%' }}>
            <LiveBarChart
              signal="FFT Computer Left Channel FFT Normalized"
              yLabelPosition="left"
              barColor="rgba(54, 162, 235, 0.6)"
              xLabelMinRotation={90}
              xLabelMaxRotation={90}
              flipX={true}
              socket={socket}
            />
          </div>
          <div style={{ width: '50%', height: '100%' }}>
            <LiveBarChart
              signal="FFT Computer Right Channel FFT Normalized"
              yLabelPosition="right"
              barColor="rgba(255, 99, 132, 0.6)"
              xLabelMinRotation={90}
              xLabelMaxRotation={90}
              flipX={false}
              socket={socket}
            />
          </div>
        </div>
        <div style={column4gridStyle}>
          {/* Right Mic */}
          <div style={itemStyle}>
            Right Mic
          </div>
          <div
            style={iconstyle}
          >
            🎤
          </div>
          <div style={itemStyle}>
            <HorizontalGauge
              min={0}
              max={120}
              signal={"FFT Computer Left Channel Power SPL"}
              socket={socket}
              zones={[
                { from: 0, to: 70, color: 'green' },
                { from: 70, to: 90, color: 'yellow' },
                { from: 90, to: 120, color: 'red' },
              ]}
              tickMarks={[0, 20, 40, 60, 80, 100, 120]}
              tickMarkLabels={['0 Db','20 Db', '40 Db', '60 Db', '80 Db', '100 Db', '120 Db']}
            />
          </div>
          <div style={itemStyle}>
            <HorizontalGauge
              min={0}
              max={1}
              signal={"FFT Computer Left Channel Power Normalized"}
              socket={socket}
              zones={[
                { from: 0.0, to: 1.0, color: 'green' },
              ]}
              tickMarks={[0.0, 0.20, 0.40, 0.60, 0.80, 1.0]}
              tickMarkLabels={['0 %','20 %', '40 %', '60 %', '80 %', '100 %']}
            />
          </div>
        </div>

        {/* Min dB */}
        <div style={rowStyle}>
          <div style={labelStyle}>Minimum dB Threshold</div>
          <Incrementer
            signal="Min db"
            socket={socket}
            min={0}
            max={50}
            step={1}
            units="dB"
            holdEnabled={true}
            holdIntervalMs={100}
          />
        </div>

        {/* Max dB */}
        <div style={rowStyle}>
          <div style={labelStyle}>Maximum dB Threshold</div>
          <Incrementer
            signal="Max db"
            socket={socket}
            min={60}
            max={120}
            step={1}
            units="dB"
            holdEnabled={true}
            holdIntervalMs={100}
          />
        </div>

        {/* Reference Table */}
        <table style={tableStyle}>
          <thead>
            <tr>
              <th style={cellStyle}>dB Level</th>
              <th style={cellStyle}>Example Sound</th>
            </tr>
          </thead>
          <tbody>
            {[
              ['-80 dB', 'Noise floor'],
              ['0 dB', 'Threshold of hearing'],
              ['30 dB', 'Whisper'],
              ['50 dB', 'Quiet conversation'],
              ['70 dB', 'Vacuum cleaner'],
              ['85 dB', 'City traffic (inside car)'],
              ['110 dB', 'Jackhammer'],
              ['120 dB', 'Ambulance siren'],
              ['130 dB', 'Pain threshold'],
              ['140 dB', 'Jet engine at takeoff'],
            ].map(([level, example]) => (
              <tr key={level}>
                <td style={cellStyle}>{level}</td>
                <td style={cellStyle}>{example}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

function SettingRow({ label, children, heightPercent }: { label: string; children: React.ReactNode; heightPercent?: number }) {
  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        width: '100%',
        height: heightPercent ? `${heightPercent}%` : 'auto',
        minHeight: 'fit-content',
        overflow: 'visible',
        boxSizing: 'border-box',
      }}
    >
      <label style={{ marginBottom: 6, fontWeight: 'bold', color: 'white' }}>{label}</label>
      <div style={{ flex: 1, width: '100%' }}>{children}</div>
    </div>
  );
}

export function SettingCurrentLimitScreen({ socket }: ScreenProps) {
  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        width: '100%',
        backgroundColor: 'black',
        color: 'white',
        padding: 20,
        boxSizing: 'border-box',
        overflowY: 'auto',
      }}
    >
      {/* Top row: DualSignalPlot, taller */}
      <div style={{ flex: 5, width: '100%', marginBottom: 20 }}>
        <DualSignalPlot
          signal1="Calculated Current"
          bufferSize={2000}
          socket={socket}
          color="cyan"
          horizontalMinSignal="Current Limit"
          minimumYAxisUpperLimit={2000}
        />
      </div>

      {/* Bottom row: settings, stacked vertically */}
      <div
        style={{
          flex: 5,
          width: '100%',
          display: 'flex',
          flexDirection: 'column',
          gap: 12,
        }}
      >
        <SettingRow label="Current Draw" heightPercent={33}>
          <SignalValueTextBox
            signal="Calculated Current"
            socket={socket}
            decimalPlaces={2}
            units="mA"
          />
        </SettingRow>

        <SettingRow label="Current Limit" heightPercent={33}>
          <Incrementer
            signal="Current Limit"
            socket={socket}
            min={500}
            max={100000}
            step={500}
            units="mA"
            holdEnabled={true}
            holdIntervalMs={100}
          />
        </SettingRow>

        <SettingRow label="LED Driver Limit" heightPercent={33}>
          <Incrementer
            signal="LED Driver Limit"
            socket={socket}
            min={1}
            max={31}
            step={1}
            holdEnabled={true}
            holdIntervalMs={100}
          />
        </SettingRow>
      </div>
    </div>
  );
}

export function SettingFrequencyRenderingScreen({ socket }: ScreenProps) {
  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        width: '100%',
        minHeight: 500,
        minWidth: 250,
        backgroundColor: 'black',
        color: 'white',
        padding: 20,
        boxSizing: 'border-box',
        overflowY: 'auto',
      }}
    >
      {/* Top row: FFT Plot, taller */}
      <div style={{ flex: 5, display: 'flex', flexDirection: 'row', width: '100%', marginBottom: 20, gap: 12, minHeight: 100, minWidth: 500, }}>
        <div style={{ flex: 1, marginBottom: 20, minHeight: 100, minWidth: 125, }}>
          <LiveBarChart
            signal="FFT Computer Left Channel FFT Normalized"
            yLabelPosition="left"
            barColor="rgba(54, 162, 235, 0.6)"
            xLabelMinRotation={90}
            xLabelMaxRotation={90}
            flipX={true}
            socket={socket}
          />
        </div>
        <div style={{ flex: 1, marginBottom: 20, minHeight: 100, minWidth: 125, }}>
          <LiveBarChart
            signal="FFT Computer Right Channel FFT Normalized"
            yLabelPosition="right"
            barColor="rgba(54, 162, 235, 0.6)"
            xLabelMinRotation={90}
            xLabelMaxRotation={90}
            flipX={false}
            socket={socket}
          />
        </div>
      </div>

      {/* Bottom row: settings, stacked vertically */}
      <div
        style={{
          flex: 5,
          width: '100%',
          display: 'flex',
          flexDirection: 'column',
          gap: 12,
          minHeight: 100,
          minWidth: 250,
          overflowY: 'auto',
        }}
      >
        <SettingRow label="Minimum Render Frequency" heightPercent={33}>
          <Incrementer
            signal="Minimum Render Frequency"
            socket={socket}
            min={0}
            max={1000}
            step={20}
            units="Hz"
            holdEnabled={true}
            holdIntervalMs={100}
          />
        </SettingRow>

        <SettingRow label="Maximum Render Frequency" heightPercent={33}>
          <Incrementer
            signal="Maximum Render Frequency"
            socket={socket}
            min={4000}
            max={20000}
            step={100}
            units="Hz"
            holdEnabled={true}
            holdIntervalMs={100}
          />
        </SettingRow>
      </div>
    </div>
  );
}

