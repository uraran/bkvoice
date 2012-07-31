package com.bktalk;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

public class BkvoiceActivity extends Activity {
	final CircularBuffer<byte[]> cBuffer = new CircularBuffer<byte[]>(2048);  

	private static final String AudioName = "/sdcard/recoder.pcm";  
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        (new Thread(new UDPRecvThread())).start();
    }
    
    public class UDPRecvThread implements Runnable
    {
		@Override
		public void run() 
		{
			Integer port = 8302;
			InetAddress remoteAddress;
			byte sendbuff[] = new byte[61];
			byte rcvbuff[] = new byte[1024];
			
			FileOutputStream fos = null;
			
			File file = new File(AudioName);  
			
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
					fos.write(rcvbuff, 0, rcvPacket.getLength());
					cBuffer.putElement(rcvbuff);
					//Log.i("元素数量", String.valueOf(cBuffer.capacity));
					Log.i("收到数据", String.valueOf(rcvPacket.getLength())); 
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
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