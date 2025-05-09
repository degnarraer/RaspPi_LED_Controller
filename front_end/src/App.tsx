import { useState, useContext, useEffect } from 'react';
import { Button, Drawer, Menu } from 'antd';
import { WebSocketContext } from './components/WebSocketContext';
import LiveBarChart from './components/LiveBarChart';
import MirroredVerticalBarChart from './components/MirroredVerticalBarChart';
import StreamingScatterPlot from './components/StreamingScatterPlot';
import ScrollingHeatmap from './components/ScrollingHeatMap';
import { RenderTickProvider } from './components/RenderingTick';
import LEDBoardTempGauge from './components/LEDBoardTempGauge';
import LedRow from './components/LedRow';
//import SignalValueTextBox from './components/SignalValueTextbox';
import HorizontalGauge from './components/HorizontalGauge';

const SCREENS = {
  HOME: 'home',
  TOWER_SCREEN: 'tower screen',
  HORIZONTAL_STEREO_SPECTRUM: 'horrizontal stereo spectrum',
  VERTICAL_STEREO_SPECTRUM: 'vertical stereo spectrum',
  LEFT_CHANNEL_WAVE: 'left channel wave screen',
  RIGHT_CHANNEL_WAVE: 'right channel wave screen',
  SCROLLING_HEAT_MAP: 'scrolling heat map screen',
};

function App() {
  const socket = useContext(WebSocketContext);
  const [visible, setVisible] = useState(false);
  const [screen, setScreen] = useState(SCREENS.HOME);

  const openDrawer = () => setVisible(true);
  const closeDrawer = () => setVisible(false);

  useEffect(() => {
    const handleEsc = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        closeDrawer();
      }
    };

    window.addEventListener('keydown', handleEsc);
    return () => {
      window.removeEventListener('keydown', handleEsc);
    };
  }, []);

  const menuItems = [
    { key: '1', screen: SCREENS.HOME, label: 'Home' },
    { key: '2', screen: SCREENS.TOWER_SCREEN, label: 'Tower Screen' },
    { key: '3', screen: SCREENS.HORIZONTAL_STEREO_SPECTRUM, label: 'Stereo Spectrum' },
    { key: '4', screen: SCREENS.VERTICAL_STEREO_SPECTRUM, label: 'Vertical Stereo Spectrum' },
    { key: '5', screen: SCREENS.LEFT_CHANNEL_WAVE, label: 'Left Channel Wave' },
    { key: '6', screen: SCREENS.RIGHT_CHANNEL_WAVE, label: 'Right Channel Wave' },
    { key: '7', screen: SCREENS.SCROLLING_HEAT_MAP, label: 'Heat Map' },
  ];

  const renderScreen = () => {
    switch (screen) {
      case SCREENS.HOME:
        return <HomeScreen />;
      case SCREENS.TOWER_SCREEN:
        return <TowerScreen />;
      case SCREENS.HORIZONTAL_STEREO_SPECTRUM:
        return <HorizontalStereoSpectrumScreen />;
      case SCREENS.VERTICAL_STEREO_SPECTRUM:
        return <VerticalStereoSpectrumScreen />;
      case SCREENS.LEFT_CHANNEL_WAVE:
        return <LeftChannelWaveScreen />;
      case SCREENS.RIGHT_CHANNEL_WAVE:
        return <RightChannelWaveScreen />;
      case SCREENS.SCROLLING_HEAT_MAP:
        return <ScrollingHeatMapScreen />;
      default:
        return <HomeScreen />;
    }
  };


  function HomeScreen() {
    return(<div style={{ width: '100%', height: '100%' }}/>)
  }

  function TowerScreen() {
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

  function HorizontalStereoSpectrumScreen() {
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

  function VerticalStereoSpectrumScreen() {
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

  function LeftChannelWaveScreen() {
    return (
      <div style={{ width: '100%', height: '100%' }}>
        <StreamingScatterPlot signal="Microphone Left Channel" socket={socket} />
      </div>
    );
  }

  function RightChannelWaveScreen() {
    return (
      <div style={{ width: '100%', height: '100%' }}>
        <StreamingScatterPlot signal="Microphone Right Channel" socket={socket} />
      </div>
    );
  }

  function ScrollingHeatMapScreen() {
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
              socket={socket}
            />
          </div>
        </div>
      </RenderTickProvider>
    );
  }

  return (
    <div style={{ width: '100%', height: '100%', position: 'absolute', top: 0, left: 0 }}>
      <Drawer title="Navigation" placement="right" onClose={closeDrawer} visible={visible}>
        <Menu style={{ zIndex: 20001 }}>
          {menuItems.map(item => (
            <Menu.Item key={item.key} onClick={() => { setScreen(item.screen); closeDrawer(); }}>
              {item.label}
            </Menu.Item>
          ))}
        </Menu>
      </Drawer>

      {renderScreen()}

      <Button
        type="primary"
        onClick={openDrawer}
        style={{
          position: 'fixed',
          top: '10px',
          right: '10px',
          zIndex: 10000,
        }}
      >
        Menu
      </Button>
    </div>
  );
}

export default App;
