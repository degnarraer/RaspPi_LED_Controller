import { useState, useContext } from 'react';
import { Button, Drawer, Menu } from 'antd';
import { WebSocketContext } from './components/WebSocketContext';
import LiveBarChart from './components/LiveBarChart';
import MirroredVerticalBarChart from './components/MirroredVerticalBarChart';
import StreamingScatterPlot from './components/StreamingScatterPlot';
import ScrollingHeatmap from './components/ScrollingHeatMap';

function App() {
  const socket = useContext(WebSocketContext);
  const [visible, setVisible] = useState(false);
  const [screen, setScreen] = useState('home');

  const openDrawer = () => setVisible(true);
  const closeDrawer = () => setVisible(false);

  function HomeScreen() {
    return <h1>Home Screen</h1>;
  }

  function LeftChannelSpectrumScreen() {
    return (
      <div style={{ width: '100%', height: '100%' }}>
        <LiveBarChart signal="FFT Bands Left Channel" socket={socket} />
      </div>
    );
  }

  function RightChannelSpectrumScreen() {
    return (
      <div style={{ width: '100%', height: '100%' }}>
        <LiveBarChart signal="FFT Bands Right Channel" socket={socket} />
      </div>
    );
  }

  function StereoSpectrumScreen() {
    return (
      <div style={{ width: '100%', height: '100%' }}>
        <MirroredVerticalBarChart leftSignal="FFT Bands Left Channel" rightSignal="FFT Bands Right Channel" socket={socket} />
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
      <ScrollingHeatmap
        signal="FFT Bands Left Channel"
        dataWidth={32}
        dataHeight={200}
        min={0}
        max={10}
        socket={socket}
      />
    );
  }

  return (
    <div style={{ width: '100%', height: '100%', position: 'absolute', top: 0, left: 0 }}>
      <Drawer title="Navigation" placement="right" onClose={closeDrawer} visible={visible}>
        <Menu style={{ zIndex: 20001 }}>
          <Menu.Item key="1" onClick={() => { setScreen('home'); closeDrawer(); }}>Home</Menu.Item>
          <Menu.Item key="2" onClick={() => { setScreen('right channel spectrum'); closeDrawer(); }}>Right Channel Spectrum</Menu.Item>
          <Menu.Item key="3" onClick={() => { setScreen('left channel spectrum'); closeDrawer(); }}>Left Channel Spectrum</Menu.Item>
          <Menu.Item key="4" onClick={() => { setScreen('stereo spectrum'); closeDrawer(); }}>Stereo Spectrum</Menu.Item>
          <Menu.Item key="5" onClick={() => { setScreen('left channel wave screen'); closeDrawer(); }}>Left Channel Wave</Menu.Item>
          <Menu.Item key="6" onClick={() => { setScreen('right channel wave screen'); closeDrawer(); }}>Right Channel Wave</Menu.Item>
          <Menu.Item key="7" onClick={() => { setScreen('scrolling heat map screen'); closeDrawer(); }}>Heat Map</Menu.Item>
        </Menu>
      </Drawer>

      {screen === 'home' && <HomeScreen />}
      {screen === 'right channel spectrum' && <RightChannelSpectrumScreen />}
      {screen === 'left channel spectrum' && <LeftChannelSpectrumScreen />}
      {screen === 'stereo spectrum' && <StereoSpectrumScreen />}
      {screen === 'left channel wave screen' && <LeftChannelWaveScreen />}
      {screen === 'right channel wave screen' && <RightChannelWaveScreen />}
      {screen === 'scrolling heat map screen' && <ScrollingHeatMapScreen />}

      <Button
        type="primary"
        onClick={openDrawer}
        style={{
          position: 'fixed',
          top: '10px',
          right: '10px',
          zIndex: 10000, // Ensure button is on top of everything else
        }}
      >
        Menu
      </Button>
    </div>
  );
}

export default App;
