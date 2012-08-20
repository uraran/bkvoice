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
import android.app.AlertDialog;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View.OnClickListener;
import android.widget.Button;

public class BkvoiceActivity extends Activity {
	static final int ABOUT_MENU_ITEM = 3;
	static final int EXIT_MENU_ITEM = 8;
	Button btnRecv = null;
	Button btnSend = null;
	boolean SavePCM = false;
	boolean DebugInfo = false;
	// CircularBuffer cBuffer = null;
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
		btnRecv = (Button) findViewById(R.id.btnRecv);
		btnSend = (Button) findViewById(R.id.btnSend);

		buffer = new ArrayList<byte[]>();
		// (new Thread(new TestThread())).start();
		// cBuffer = new CircularBuffer(16); //环形缓冲区

		btnRecv.setOnClickListener(btnClickListner);
		btnSend.setOnClickListener(btnClickListner);
	}

	@Override
	public void onCreateContextMenu(ContextMenu menu, View v,
			ContextMenuInfo menuInfo) {
		super.onCreateContextMenu(menu, v, menuInfo);
		menu.add("退出");
	};

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		super.onCreateOptionsMenu(menu);
		menu.add(0, ABOUT_MENU_ITEM, 0, R.string.about);
		menu.add(0, EXIT_MENU_ITEM, 0, R.string.send);
		return true;

	};

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		super.onOptionsItemSelected(item);
		if(item.getItemId() == ABOUT_MENU_ITEM)
		{
			AlertDialog m_AlertDlg = new AlertDialog.Builder(this)
			.setMessage(R.string.finish)
			.setTitle(R.string.app_name)
			.setIcon(R.drawable.icon)
			.setCancelable(true)
			.show();
		}
		else if(item.getItemId() == EXIT_MENU_ITEM)
		{
			this.finish();
		}
		return true;
	};

	OnClickListener btnClickListner = new OnClickListener() {
		@Override
		public void onClick(View v) {
			if (v.getId() == R.id.btnRecv) {
				(new Thread(new UDPRecvThread())).start();
				(new Thread(new AudioPlayThread())).start();
			} else if (v.getId() == R.id.btnSend) {
				(new Thread(new UDPRecvThread())).start();
				(new Thread(new AudioPlayThread())).start();
			}
		}

	};

	public class AudioRecordThread implements Runnable {
		@Override
		public void run() {
			// TODO Auto-generated method stub

		}

	};

	public class UDPSendThread implements Runnable {
		@Override
		public void run() {
			// TODO Auto-generated method stub

		}

	};

	public class AudioPlayThread implements Runnable {
		FileOutputStream fosplay = null;
		File file = null;

		@Override
		public void run() {
			if (SavePCM) {
				file = new File(AudioPlayName);

				try {
					fosplay = new FileOutputStream(file);
				} catch (FileNotFoundException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}
			}

			miniPlayBufSize = AudioTrack.getMinBufferSize(frequency,
					channelConfiguration, audioEncoding);

			audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, frequency,
					channelConfiguration, audioEncoding, miniPlayBufSize,
					AudioTrack.MODE_STREAM);

			// Integer offset = 0;
			Integer readLength = 0;

			audioTrack.play();

			while (true) {
				// 获取许可
				try {
					semp.acquire();
				} catch (InterruptedException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}

				if (buffer.size() > 0) {
					// Log.i("play", String.valueOf(buffer.size()));
					// int length = buffer.get(0).length;
					byte[] data = buffer.get(0);
					if (data != null) {
						int playframeNo = data[513] * 0x100 + data[512];
						if (DebugInfo)
							System.out.println("play:" + playframeNo);
						audioTrack.write(data, 0, 512);

						if (SavePCM) {
							try {
								fosplay.write(data, 0, 512);
							} catch (IOException e) {
								// TODO Auto-generated catch block
								e.printStackTrace();
							}
						}

					}
					buffer.remove(0);
				} else {
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

	public class UDPRecvThread implements Runnable {
		@Override
		public void run() {
			Integer port = 8302;
			InetAddress remoteAddress;
			byte sendbuff[] = new byte[61];
			byte rcvbuff[] = new byte[520];

			FileOutputStream fos = null;
			File file = null;
			if (SavePCM) {
				file = new File(AudioRecvName);

				try {
					fos = new FileOutputStream(file);
				} catch (FileNotFoundException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}
			}

			DatagramSocket datagramSocket = null;
			try {
				datagramSocket = new DatagramSocket(port);
			} catch (SocketException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}

			DatagramPacket datagramPacket = new DatagramPacket(sendbuff,
					sendbuff.length);
			DatagramPacket rcvPacket = new DatagramPacket(rcvbuff,
					rcvbuff.length);

			while (true) {
				rcvPacket = new DatagramPacket(rcvbuff, rcvbuff.length);
				try {
					datagramSocket.receive(rcvPacket);
					if (rcvPacket.getLength() > 0) {
						byte[] data = new byte[520];
						System.arraycopy(rcvbuff, 0, data, 0, rcvPacket
								.getLength());
						buffer.add(data);
						int recvframeNo = data[513] * 0x100 + data[512];
						if (DebugInfo)
							System.out.println("recv:" + recvframeNo);
						// 访问完后，释放
						semp.release();
					}
					// Log.i("recv", String.valueOf(buffer.size()));
					if (SavePCM) {
						fos.write(rcvPacket.getData(), 0, 512);
					}
					// cBuffer.putElement(rcvbuff);
					// Log.i("元素数量", String.valueOf(cBuffer.capacity));
					// Log.i("收到数据", String.valueOf(rcvPacket.getLength()));
				} catch (IOException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}
			}
			/*
			 * datagramPacket.setAddress(remoteAddress);
			 * datagramPacket.setPort(8302); datagramPacket.setLength(61);
			 * datagramSocket.send(datagramPacket);
			 */
		}

	}
}