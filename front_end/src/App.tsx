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
  return(
    <div><h1>Left Channel Spectrum</h1>
      <LiveBarChart signal="FFT Bands Left Channel" socket={socket}/>
    </div>
  );
}


function RightChannelSpectrumScreen() {
  return(
    <div><h1>Right Channel Spectrum</h1>
      <LiveBarChart signal="FFT Bands Right Channel" socket={socket}/>
    </div>
  );
}

function StereoSpectrumScreen() {
  return(
    <div><h1>Stereo Spectrum</h1>
      <MirroredVerticalBarChart leftSignal="FFT Bands Left Channel" rightSignal="FFT Bands Right Channel" socket={socket}/>
    </div>
  );
}

function LeftChannelWaveScreen() {
  return(
    <div><h1>Wave</h1>
      <StreamingScatterPlot signal="Microphone Left Channel" socket={socket}/>
    </div>
  );
}

function RightChannelWaveScreen() {
  return(
    <div><h1>Wave</h1>
      <StreamingScatterPlot signal="Microphone Right Channel" socket={socket}/>
    </div>
  );
}

function ScrollingHeatMapScreen() {
  return(
    <div><h1>Scrolling Heat Map</h1>
      <ScrollingHeatmap
        signal="FFT Bands Left Channel"
        width={500}
        height={200}
        min={0}
        max={10}
        socket={socket}
      />
    </div>
  );
}


  return (
    <div>
      <div>
        <Button type="primary" onClick={openDrawer}>
          Open Menu
        </Button>
        <Drawer title="Navigation" placement="left" onClose={closeDrawer} visible={visible}>
          <Menu>
            <Menu.Item key="1" onClick={() => { setScreen('home'); closeDrawer(); }}>Home</Menu.Item>
            <Menu.Item key="2" onClick={() => { setScreen('right channel spectrum'); closeDrawer(); }}>Right Channel Spectrum</Menu.Item>
            <Menu.Item key="3" onClick={() => { setScreen('left channel spectrum'); closeDrawer(); }}>Left Channel Spectrum</Menu.Item>
            <Menu.Item key="4" onClick={() => { setScreen('stereo spectrum'); closeDrawer(); }}>Stereo Spectrum</Menu.Item>
            <Menu.Item key="5" onClick={() => { setScreen('left channel wave screen'); closeDrawer(); }}>Left Channel Wave</Menu.Item>
            <Menu.Item key="6" onClick={() => { setScreen('right channel wave screen'); closeDrawer(); }}>Right Channel Wave</Menu.Item>
            <Menu.Item key="7" onClick={() => { setScreen('scrolling heat map screen'); closeDrawer(); }}>Scrolling Heat Map</Menu.Item>
          </Menu>
        </Drawer>
      </div>
        {screen === 'home' && <HomeScreen />}
        {screen === 'right channel spectrum' && <RightChannelSpectrumScreen />}
        {screen === 'left channel spectrum' && <LeftChannelSpectrumScreen />}
        {screen === 'stereo spectrum' && <StereoSpectrumScreen/>}
        {screen === 'left channel wave screen' && <LeftChannelWaveScreen/>}
        {screen === 'right channel wave screen' && <RightChannelWaveScreen/>}
        {screen === 'scrolling heat map screen' && <ScrollingHeatMapScreen/>}
    </div>
  );
}

export default App;
