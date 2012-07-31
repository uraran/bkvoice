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
	
	private static final String AudioRecvName = "/sdcard/recoder_recv.pcm";  
	private static final String AudioPlayName = "/sdcard/recoder_play.pcm";  
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        buffer = new ArrayList<byte[]>();
        //cBuffer = new CircularBuffer(16);  //环形缓冲区
        (new Thread(new UDPRecvThread())).start();
        (new Thread(new AudioPlayThread())).start();
    }
    
    public class AudioPlayThread implements Runnable
    {
    	FileOutputStream fos = null;
    	
		@Override
		public void run() {
/*
			File file = new File(AudioPlayName);  
			
			try {
				fos = new FileOutputStream(file);
			} catch (FileNotFoundException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}
*/
			miniPlayBufSize = AudioTrack.getMinBufferSize(frequency,
					channelConfiguration, audioEncoding);
			
			audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, frequency,
					channelConfiguration, audioEncoding,
					miniPlayBufSize, AudioTrack.MODE_STREAM);
			
			
			//Integer offset = 0;
			Integer readLength = 0;
			
			audioTrack.play(); 
			
			while(true)
			{			/*	
				try {
					fos.write(buffer.get(0), 0, 160);
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				*/
				if(buffer.size() > 0)
				{
					audioTrack.write(buffer.get(0), 0, 160);
					buffer.remove(0);
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
				try {
					datagramSocket.receive(rcvPacket);
					buffer.add(rcvbuff);
					fos.write(rcvbuff, 0, rcvPacket.getLength());
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