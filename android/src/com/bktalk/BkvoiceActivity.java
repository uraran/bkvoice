package com.bktalk;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.concurrent.Semaphore;

import android.app.Activity;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.util.Log;

public class BkvoiceActivity extends Activity {
	//CircularBuffer cBuffer = null;
	ArrayList<byte[]> buffer = null;
	static final int frequency = 16000;
	static final int channelConfiguration = AudioFormat.CHANNEL_CONFIGURATION_MONO;
	static final int audioEncoding = AudioFormat.ENCODING_PCM_16BIT;
	AudioTrack audioTrack;
	int miniPlayBufSize;
	
	final Semaphore semp = new Semaphore(5);  
	
	private static final String AudioRecvName = "/sdcard/recoder_recv.pcm";  
	private static final String AudioPlayName = "/sdcard/recoder_play_111.pcm";  
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        buffer = new ArrayList<byte[]>();
        //(new Thread(new TestThread())).start();
        //cBuffer = new CircularBuffer(16);  //环形缓冲区
        (new Thread(new UDPRecvThread())).start();
        (new Thread(new AudioPlayThread())).start();
    }
    
    public class TestThread implements Runnable
    {
		@Override
		public void run() {
			ArrayList<byte[]> list = new ArrayList<byte[]>(0);
			byte[] data = new byte[20];
			data[0] = 0;
			data[1] = -100;
			data[2] = (byte) -200;
			data[3] = -100;
			data[4] = -100;
			list.add(data);
			
			data = new byte[20];
			data[0] = 1;
			list.add(data);
			
			data = new byte[20];
			data[0] = 2;
			list.add(data);
			
			data = new byte[20];
			data[0] = 3;
			list.add(data);
			
			data = new byte[20];
			data[0] = 4;
			list.add(data);
			
			byte[] dataread = list.get(0);
			list.remove(0);
			Log.i("test", String.valueOf(dataread.length));
			
			dataread = list.get(0);
			list.remove(0);
			Log.i("test", String.valueOf(dataread.length));
			
			dataread = list.get(0);
			list.remove(0);
			Log.i("test", String.valueOf(dataread.length));
			
			dataread = list.get(0);
			list.remove(0);
			Log.i("test", String.valueOf(dataread.length));
			
			dataread = list.get(0);
			list.remove(0);
			Log.i("test", String.valueOf(dataread.length));
		}
    	
    }
    public class AudioPlayThread implements Runnable
    {
    	FileOutputStream fosplay = null;
    	
		@Override
		public void run() {

			File file = new File(AudioPlayName);  
			
			try {
				fosplay = new FileOutputStream(file);
			} catch (FileNotFoundException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}

			miniPlayBufSize = AudioTrack.getMinBufferSize(frequency,
					channelConfiguration, audioEncoding);
			
			audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, frequency,
					channelConfiguration, audioEncoding,
					miniPlayBufSize, AudioTrack.MODE_STREAM);
			
			
			//Integer offset = 0;
			Integer readLength = 0;
			
			audioTrack.play(); 
			
			while(true)
			{	
				// 获取许可  
                try {
					semp.acquire();
				} catch (InterruptedException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}  
                
				if(buffer.size() > 5)
				{
					//Log.i("play", String.valueOf(buffer.size()));
					//int length = buffer.get(0).length;
					byte[] data = buffer.get(0);
					if(data != null)
					{
						audioTrack.write(data, 0, data.length);
						
						/*
						try {
							fosplay.write(data, 0, data.length);
						} catch (IOException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
						*/
						
					}
					buffer.remove(0);
				}
				else
				{
					try {
						Thread.sleep(10);
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				}
			}
		}
    	
    }
    
    public class UDPRecvThread implements Runnable
    {
		@Override
		public void run() 
		{
			Integer port = 8302;
			InetAddress remoteAddress;
			byte sendbuff[] = new byte[61];
			byte rcvbuff[] = new byte[160];
			
			FileOutputStream fos = null;
			
			File file = new File(AudioRecvName);  
			
			try {
				fos = new FileOutputStream(file);
			} catch (FileNotFoundException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}
			
			
			DatagramSocket datagramSocket = null;
			try {
				datagramSocket = new DatagramSocket(port);
			} catch (SocketException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			
			DatagramPacket  datagramPacket = new DatagramPacket(sendbuff, sendbuff.length);
			DatagramPacket  rcvPacket = new DatagramPacket(rcvbuff, rcvbuff.length);
			
			while(true)
			{				
				rcvPacket = new DatagramPacket(rcvbuff, rcvbuff.length);
				try {
					datagramSocket.receive(rcvPacket);
					if(rcvPacket.getLength() > 0)
					{
						byte[] data = new byte[160];
						System.arraycopy(rcvbuff, 0, data, 0, rcvPacket.getLength());
						buffer.add(data);
						
						// 访问完后，释放  
                        semp.release(); 
					}
					//Log.i("recv", String.valueOf(buffer.size()));
					//fos.write(rcvPacket.getData(), 0, rcvPacket.getLength());
					//cBuffer.putElement(rcvbuff);
					//Log.i("元素数量", String.valueOf(cBuffer.capacity));
					//Log.i("收到数据", String.valueOf(rcvPacket.getLength())); 
				} catch (IOException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}
			}
			/*
			datagramPacket.setAddress(remoteAddress);
			datagramPacket.setPort(8302);
			datagramPacket.setLength(61);
			datagramSocket.send(datagramPacket);
			*/
		}
    	
    }
}