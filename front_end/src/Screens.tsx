
import LiveBarChart from './components/LiveBarChart';
import MirroredVerticalBarChart from './components/MirroredVerticalBarChart';
import StreamingScatterPlot from './components/StreamingScatterPlot';
import ScrollingHeatmap from './components/ScrollingHeatMap';
import { RenderTickProvider } from './components/RenderingTick';
import LEDBoardTempGauge from './components/LEDBoardTempGauge';
import LedRow from './components/LedRow';
//import SignalValueTextBox from './components/SignalValueTextbox';
import HorizontalGauge from './components/HorizontalGauge';
import { WebSocketContextType } from './components/WebSocketContext';

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
  SETTING_BRIGHTNESS: 'setting brightness',
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
        case SCREENS.SCROLLING_HEAT_MAP:
        return <ScrollingHeatMapScreen socket={socket} />;
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
        {Array.from({ length: 64 }, (_, i) => (
          <>
            <div key={`led-${i}`} style={itemStyle}>
              <LedRow ledCount={32} signal="Pixel Grid" rowIndex={i} socket={socket} randomMode={false} />
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
          socket={socket} />
      </div>
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
              dataHeight={240}
              min={0}
              max={10}
              flipX={true}
              mode={'Rainbow'}
              socket={socket}
            />
          </div>
          <div style={{ width: '50%', height: '100%' }}>
            <ScrollingHeatmap
              signal="FFT Bands Right Channel"
              dataWidth={32}
              dataHeight={240}
              min={0}
              max={10}
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
