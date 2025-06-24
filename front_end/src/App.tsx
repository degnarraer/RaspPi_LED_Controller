import { useState, useContext, useEffect } from 'react';
import { Button, Drawer, Menu } from 'antd';
import { WebSocketContext } from './components/WebSocketContext';
import { renderScreen, ScreenType, SCREENS } from './Screens.tsx';
import Current from './assets/current.png';
import Color from './assets/color.png';
import Microphone from './assets/microphone.png';

import {
  MenuOutlined,
  SettingOutlined,
  HomeOutlined,
  BarChartOutlined,
  TableOutlined,
  HeatMapOutlined,
  LineChartOutlined,
  PlayCircleOutlined,
  ArrowLeftOutlined,
} from '@ant-design/icons';

interface MenuItem {
  key: string;
  targetScreen?: ScreenType;
  targetMenu?: 'main' | 'settings' | 'animations' | 'heatmap';
  label: string;
  icon: React.ReactNode;
}

const MENU_STRUCTURE = {
  main: {
    label: 'Navigation',
    items: [
      { key: 'home', targetScreen: SCREENS.HOME, label: 'Home', icon: <HomeOutlined style={{ fontSize: '50px' }} /> },
      { key: 'settings', targetMenu: 'settings', label: 'Settings', icon: <SettingOutlined style={{ fontSize: '50px' }} /> },
      { key: 'animations', targetMenu: 'animations', label: 'Animations', icon: <PlayCircleOutlined style={{ fontSize: '50px' }} /> },
    ],
  },
  settings: {
    label: 'Settings',
    items: [
      { key: 'Color', targetScreen: SCREENS.SETTING_RENDERING, label: 'Color', icon: <img src={Color} alt="Color" style={{ maxWidth: '50px', maxHeight: '50px', objectFit: 'contain', }} />  },
      { key: 'Current Limit', targetScreen: SCREENS.SETTING_CURRENT_LIMIT, label: 'Current Limit', icon: <img src={Current} alt="Current Limit" style={{ maxWidth: '50px', maxHeight: '50px', objectFit: 'contain', }} /> },
      { key: 'Microphone', targetScreen: SCREENS.SETTING_SENSITIVITY, label: 'Microphone', icon: <img src={Microphone} alt="Microphone" style={{ maxWidth: '50px', maxHeight: '50px', objectFit: 'contain', }} /> },
      { key: 'back', targetMenu: 'main', label: 'Back', icon: <ArrowLeftOutlined style={{ fontSize: '50px' }} /> },
    ],
  },
  animations: {
    label: 'Animations',
    items: [
      { key: 'tower screen', targetScreen: SCREENS.TOWER_SCREEN, label: 'Tower Screen', icon: <TableOutlined style={{ fontSize: '50px' }} /> },
      { key: 'horizontal stereo spectrum', targetScreen: SCREENS.HORIZONTAL_STEREO_SPECTRUM, label: 'Stereo Spectrum', icon: <BarChartOutlined style={{ fontSize: '50px' }} /> },
      { key: 'vertical stereo spectrum', targetScreen: SCREENS.VERTICAL_STEREO_SPECTRUM, label: 'Vertical Stereo Spectrum', icon: <BarChartOutlined style={{ fontSize: '50px', transform: 'scaleX(-1) rotate(-90deg)' }} /> },
      { key: 'wave screen', targetScreen: SCREENS.WAVE_SCREEN, label: 'Wave Screen', icon: <LineChartOutlined style={{ fontSize: '50px' }} /> },
      { key: 'heat map', targetMenu: 'heatmap', label: 'Heat Map', icon: <HeatMapOutlined style={{ fontSize: '50px' }} /> },
      { key: 'back', targetMenu: 'main', label: 'Back', icon: <ArrowLeftOutlined style={{ fontSize: '50px' }} /> },
    ],
  },
  heatmap: {
    label: 'Heat Map',
    items: [
      { key: 'scrolling heat map screen', targetScreen: SCREENS.SCROLLING_HEAT_MAP, label: 'Normal', icon: <HeatMapOutlined style={{ fontSize: '50px' }} /> },
      { key: 'scrolling heat map rainbow screen', targetScreen: SCREENS.SCROLLING_HEAT_MAP_RAINBOW, label: 'Rainbow', icon: <HeatMapOutlined style={{ fontSize: '50px' }} /> },
      { key: 'back', targetMenu: 'animations', label: 'Back', icon: <ArrowLeftOutlined style={{ fontSize: '50px' }} /> },
    ],
  },
} as const;


function App() {
  const socket = useContext(WebSocketContext);
  const [visible, setVisible] = useState(false);
  const [screen, setScreen] = useState<ScreenType>(SCREENS.HOME);
  const menuKeys = Object.keys(MENU_STRUCTURE) as Array<keyof typeof MENU_STRUCTURE>;
  const [menuStack, setMenuStack] = useState<(keyof typeof MENU_STRUCTURE)[]>([menuKeys[0]]);
  const currentMenuKey = menuStack[menuStack.length - 1];
  const currentMenu = MENU_STRUCTURE[currentMenuKey];

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

  const handleMenuClick = (item: MenuItem) => {
    if ('targetScreen' in item && item.targetScreen) {
      setScreen(item.targetScreen);
      setVisible(false);
    } else if ('targetMenu' in item && item.targetMenu) {
      if (item.label.toLowerCase() === 'back') {
        setMenuStack(prev => (prev.length > 1 ? prev.slice(0, -1) : prev));
      } else {
        setMenuStack(prev => [...prev, item.targetMenu as keyof typeof MENU_STRUCTURE]);
      }
    }
  };

  function useWindowHeight() {
    const [height, setHeight] = useState(window.innerHeight);

    useEffect(() => {
      const handleResize = () => setHeight(window.innerHeight);
      window.addEventListener('resize', handleResize);
      window.addEventListener('orientationchange', handleResize);
      return () => {
        window.removeEventListener('resize', handleResize);
        window.removeEventListener('orientationchange', handleResize);
      };
    }, []);

    return height;
  }
  const height = useWindowHeight();

  return (
    <div
      style={{
        width: '100vw',
        height: height,
        display: 'flex',
        flexDirection: 'column',
        background: 'linear-gradient(135deg,rgb(104, 104, 104),rgb(0, 0, 0))',
        color: 'white',
        fontFamily: 'Arial, sans-serif',
        boxSizing: 'border-box',
        overflow: 'hidden',
      }}
    >
      {/* Header with Notched Menu Button */}
      <div
        style={{
          height: '40px',
          flexShrink: 0,
          display: 'flex',
          justifyContent: 'flex-end',
          alignItems: 'center',
          padding: '0 20px',
          backgroundColor: '#1c1c1c',
          boxShadow: '0 2px 4px rgba(0, 0, 0, 0.4)',
          boxSizing: 'border-box',
        }}
      >
        <Button
          type="primary"
          onClick={openDrawer}
          aria-label="Open Menu"
          icon={<MenuOutlined style={{ fontSize: '18px' }} />}
          style={{
            position: 'absolute',
            top: 0,
            right: 0,
            height: '40px',
            padding: '0 16px',
            backgroundColor: '#1890ff',
            borderColor: '#1890ff',
            color: 'white',
            fontWeight: 'bold',
            borderRadius: 0,
            clipPath: 'polygon(0 0, 100% 0, 100% 100%, 12px 100%, 0 calc(100% - 12px))',
            boxSizing: 'border-box',
          }}
        >
          Menu
        </Button>

      </div>

      {/* Main Screen Content */}
      <div
        style={{
          flex: 1,
          display: 'flex',
          justifyContent: 'center',
          alignItems: 'center',
          boxSizing: 'border-box',
          overflow: 'hidden',
        }}
      >
        <div
          style={{
            width: '100%',
            height: '100%',
            backgroundColor: 'rgba(255, 255, 255, 0.03)',
            borderRadius: '16px',
            padding: '24px',
            boxShadow: '0 0 12px rgba(0,0,0,0.3)',
            backdropFilter: 'blur(4px)',
            animation: 'fadeIn 0.4s ease-in-out',
            boxSizing: 'border-box',
            overflow: 'hidden',
          }}
        >
          {renderScreen({ socket, screen })}
        </div>
      </div>

      {/* Drawer Menu - keep it inside layout to avoid scroll */}
      <Drawer
        title={currentMenu.label}
        placement="right"
        onClose={() => setVisible(false)}
        open={visible}
        bodyStyle={{ padding: 0 }}
        style={{ position: 'absolute' }}
        getContainer={false} // render inside this component, not the body
      >
        <Menu mode="vertical" style={{ borderRight: 0 }}>
          {currentMenu.items.map((item) => (
            <Menu.Item
              key={item.key}
              icon={item.icon}
              onClick={() => handleMenuClick(item)}
              style={{
                padding: '10px 16px',
                fontSize: '16px',
                whiteSpace: 'normal',
                boxSizing: 'border-box',
                height: '60px',
              }}
            >
              {item.label}
            </Menu.Item>
          ))}
        </Menu>
      </Drawer>

      <style>
        {`
          html, body {
            margin: 0;
            padding: 0;
            height: 100%;
            overflow: hidden;
          }
          @keyframes fadeIn {
            from { opacity: 0; transform: translateY(10px); }
            to { opacity: 1; transform: translateY(0); }
          }
        `}
      </style>
    </div>
  );
}

export default App;

