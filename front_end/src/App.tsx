import { useState, useContext } from 'react';
import { Button, Drawer, Menu } from 'antd';
import { WebSocketContext } from './components/WebSocketContext';
import LiveBarChart from './components/LiveBarChart';
import MirroredVerticalBarChart from './components/MirroredVerticalBarChart';

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
  const labels = [
    '16 Hz', '20 Hz', '25 Hz', '31.5 Hz', '40 Hz', '50 Hz', '63 Hz', '80 Hz', '100 Hz', '125 Hz', 
    '160 Hz', '200 Hz', '250 Hz', '315 Hz', '400 Hz', '500 Hz', '630 Hz', '800 Hz', '1000 Hz', '1250 Hz', 
    '1600 Hz', '2000 Hz', '2500 Hz', '3150 Hz', '4000 Hz', '5000 Hz', '6300 Hz', '8000 Hz', '10000 Hz', 
    '12500 Hz', '16000 Hz', '20000 Hz'
  ];
  
  const initialData = [
    0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 
    2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2
  ];
  return(
    <div><h1>Left Channel Spectrum</h1>
      <LiveBarChart labels={labels} initialData={initialData} signal="FFT Bands Left Channel" socket={socket}/>
    </div>
  );
}


function RightChannelSpectrumScreen() {
  const labels = [
    '16 Hz', '20 Hz', '25 Hz', '31.5 Hz', '40 Hz', '50 Hz', '63 Hz', '80 Hz', '100 Hz', '125 Hz', 
    '160 Hz', '200 Hz', '250 Hz', '315 Hz', '400 Hz', '500 Hz', '630 Hz', '800 Hz', '1000 Hz', '1250 Hz', 
    '1600 Hz', '2000 Hz', '2500 Hz', '3150 Hz', '4000 Hz', '5000 Hz', '6300 Hz', '8000 Hz', '10000 Hz', 
    '12500 Hz', '16000 Hz', '20000 Hz'
  ];
  
  const initialData = [
    0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 
    2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2
  ];
  return(
    <div><h1>Right Channel Spectrum</h1>
      <LiveBarChart labels={labels} initialData={initialData} signal="FFT Bands Right Channel" socket={socket}/>
    </div>
  );
}


function StereoSpectrumScreen() {
  const labels = [
    '16 Hz', '20 Hz', '25 Hz', '31.5 Hz', '40 Hz', '50 Hz', '63 Hz', '80 Hz', '100 Hz', '125 Hz', 
    '160 Hz', '200 Hz', '250 Hz', '315 Hz', '400 Hz', '500 Hz', '630 Hz', '800 Hz', '1000 Hz', '1250 Hz', 
    '1600 Hz', '2000 Hz', '2500 Hz', '3150 Hz', '4000 Hz', '5000 Hz', '6300 Hz', '8000 Hz', '10000 Hz', 
    '12500 Hz', '16000 Hz', '20000 Hz'
  ];
  
  const initialData = [
    0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 
    2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2
  ];
  return(
    <div><h1>Stereo Spectrum</h1>
      <MirroredVerticalBarChart labels={labels} initialLeft={initialData} initialRight={initialData} signalLeft="FFT Bands Left Channel" signalRight="FFT Bands Right Channel" socket={socket}/>
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
          </Menu>
        </Drawer>
      </div>
        {screen === 'home' && <HomeScreen />}
        {screen === 'right channel spectrum' && <RightChannelSpectrumScreen />}
        {screen === 'left channel spectrum' && <LeftChannelSpectrumScreen />}
        {screen === 'stereo spectrum' && <StereoSpectrumScreen/>}
    </div>
  );
}

export default App;
