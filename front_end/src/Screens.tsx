
import LiveBarChart from './components/LiveBarChart';
import MirroredVerticalBarChart from './components/MirroredVerticalBarChart';
import StreamingScatterPlot from './components/StreamingScatterPlot';
import ScrollingHeatmap from './components/ScrollingHeatMap';
import { RenderTickProvider } from './components/RenderingTick';
import LEDBoardTempGauge from './components/LEDBoardTempGauge';
import LedRow from './components/LedRow';
import SignalValueTextBox from './components/SignalValueTextbox';
import HorizontalGauge from './components/HorizontalGauge';
import { WebSocketContextType } from './components/WebSocketContext';
import Incrementer from './components/Incrementer';
import ValueSelector from './components/ValueSelector';
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
        default:
        return <HomeScreen />;
    }
};

export function HomeScreen() {
    return(<div style={{ width: '100%', height: '100%' }}/>)
}

export function TowerScreen({ socket }: ScreenProps) {
    const gridStyle = {
        width: '100%',
        height:'100%',
        display: 'grid',
        gridTemplateColumns: 'auto auto',
        gridTemplateRows: 'auto auto',
        gap: '0px',
        backgroundColor: 'black',
        padding: '0px',
    };
  
    const itemStyle = {
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
            <div key={`led-${i}`} style={itemStyle}>
              <LedRow ledCount={5} signal="Pixel Grid" rowIndex={i} socket={socket} randomMode={false} />
            </div>
            <div key={`gauge-${i}`} style={itemStyle}>
              <LEDBoardTempGauge signalName={"Temp Signal {i}"} socket={socket} />
            </div>
          </>
        ))}
        <div style={itemStyle}>
          CPU Usage
        </div>
        <div style={itemStyle}>
          <HorizontalGauge min={0} max={100} signal={"CPU Usage"} socket={socket} zones={[{ from: 0, to: 100, color: 'green' },]}tickMarks={[10, 20, 30, 40, 50, 60, 70, 80, 90,]}/>
        </div>
        <div style={itemStyle}>
          Memory Usage
        </div>
        <div style={itemStyle}>
          <HorizontalGauge min={0} max={100} signal={"CPU Memory Usage"} socket={socket} zones={[{ from: 0, to: 100, color: 'green' },]}tickMarks={[10, 20, 30, 40, 50, 60, 70, 80, 90,]}/>
        </div>
        <div style={itemStyle}>
          CPU Temp
        </div>
        <div style={itemStyle}>
          <HorizontalGauge min={30} max={90} signal={"CPU Temp"} socket={socket} zones={[{ from: 0, to: 70, color: 'green' }, { from: 70, to: 80, color: 'yellow' },{ from: 80, to: 90, color: 'red' },]} tickMarks={[40, 50, 60, 70, 80,]}/>
        </div>
      </div>
    );
  }

  export function HorizontalStereoSpectrumScreen({ socket }: ScreenProps) {
    return (
      <div style={{ display: 'flex', width: '100%', height: '100%' }}>
        <div style={{ width: '50%', height: '100%' }}>
          <LiveBarChart
            signal="FFT Bands Left Channel"
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
            signal="FFT Bands Right Channel"
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
          leftSignal="FFT Bands Left Channel"
          rightSignal="FFT Bands Right Channel"
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
              signal="FFT Bands Left Channel"
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
              signal="FFT Bands Right Channel"
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
              signal="FFT Bands Left Channel"
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
              signal="FFT Bands Right Channel"
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
  return (
        <div style={{ 
        display: 'flex', 
        height: '100vh', 
        width: '100vw', 
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
      <div style={{ width: '100%', display: 'flex', flexDirection: 'column', gap: 20 }}>

        {/* Row for Color Mapping */}
        <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'center', gap: 10, justifyContent: 'center' }}>
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
  const cellStyle: React.CSSProperties = {
    border: '1px solid #444',
    padding: '8px',
    textAlign: 'left',
  };

  return (
        <div style={{ 
        display: 'flex', 
        height: '100vh', 
        width: '100vw', 
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
      <div style={{ width: '100%', display: 'flex', flexDirection: 'column', gap: 20 }}>

        {/* Minimum dB Threshold */}
        <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'center', gap: 10, justifyContent: 'center' }}>
          <div style={{ width: 200, textAlign: 'right' }}>
            <h2 style={{ margin: 0, userSelect: 'none' }}>Minimum dB Threshold</h2>
          </div>
          <Incrementer signal="Min db" socket={socket} min={-80} max={30} step={1} units="dB" holdEnabled={true} holdIntervalMs={100} />
        </div>

        {/* Maximum dB Threshold */}
        <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'center', gap: 10, justifyContent: 'center' }}>
          <div style={{ width: 200, textAlign: 'right' }}>
            <h2 style={{ margin: 0, userSelect: 'none' }}>Maximum dB Threshold</h2>
          </div>
          <Incrementer signal="Max db" socket={socket} min={0} max={140} step={1} units="dB" holdEnabled={true} holdIntervalMs={100} />
        </div>

        {/* Reference Table */}
        <table style={{
          borderCollapse: 'collapse',
          width: '100%',
          backgroundColor: '#222',
          color: 'white',
          fontSize: 14,
          marginTop: 20
        }}>
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

export function SettingCurrentLimitScreen({ socket }: ScreenProps) {
  return (
        <div style={{ 
        display: 'flex', 
        height: '100vh', 
        width: '100vw', 
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
      <div style={{ width: '100%', display: 'flex', flexDirection: 'column', gap: 20 }}>
        {/* Current Draw */}
        <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'center', gap: 10, justifyContent: 'center' }}>
          <div style={{ width: 200, textAlign: 'right' }}>
            <h2 style={{ margin: 0, userSelect: 'none' }}>Current Draw</h2>
          </div>
          <SignalValueTextBox signal="Calculated Current" socket={socket} decimalPlaces={2} units="mA" />
        </div>

        {/* Current Limit */}
        <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'center', gap: 10, justifyContent: 'center' }}>
          <div style={{ width: 200, textAlign: 'right' }}>
            <h2 style={{ margin: 0, userSelect: 'none' }}>Current Limit</h2>
          </div>
          <Incrementer signal="Current Limit" socket={socket} min={500} max={100000} step={500} units="mA" holdEnabled={true} holdIntervalMs={10} />
        </div>

        {/* LED Driver Limit */}
        <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'center', gap: 10, justifyContent: 'center' }}>
          <div style={{ width: 200, textAlign: 'right' }}>
            <h2 style={{ margin: 0, userSelect: 'none' }}>LED Driver Limit</h2>
          </div>
          <Incrementer signal="LED Driver Limit" socket={socket} min={1} max={31} step={1} holdEnabled={true} holdIntervalMs={100} />
        </div>
      </div>
    </div>
  );
}