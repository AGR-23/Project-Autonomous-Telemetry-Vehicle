package net;

import model.TelemetryModel;

import javafx.application.Platform;

import java.io.*;
import java.net.Socket;
import java.net.SocketException;
import java.time.Instant;
import java.util.Map;
import java.util.concurrent.*;
import java.util.function.Consumer;

/**
 * Network client for connecting to the telemetry server and processing incoming data.
 * This class manages the network connection, handles automatic reconnection with backoff,
 * and processes telemetry messages to update the model.
 * 
 * <p>Key features:
 * <ul>
 *   <li>Automatic connection with exponential backoff retry</li>
 *   <li>Asynchronous message processing on JavaFX Application Thread</li>
 *   <li>Clean shutdown and resource management</li>
 *   <li>Status callbacks for UI updates</li>
 *   <li>Comprehensive logging</li>
 * </ul>
 * 
 * <p>The client follows the telemetry protocol:
 * <ol>
 *   <li>Sends HELLO message with client name upon connection</li>
 *   <li>Listens for TLM messages and updates the model</li>
 *   <li>Handles OK/ERR/BYE responses</li>
 * </ol>
 * 
 * @author Autonomous Vehicle Team
 * @version 1.0
 * @since 2025
 */
public class NetworkClient {
    /** Server hostname or IP address. */
    private final String host;
    
    /** Server port number. */
    private final int port;
    
    /** Client identifier name. */
    private final String clientName;
    
    /** Telemetry data model to update. */
    private final TelemetryModel model;
    
    /** Callback function for appending log messages. */
    private final Consumer<String> logCb;     // append log
    
    /** Callback function for setting status messages. */
    private final Consumer<String> statusCb;  // set status label

    /** Flag indicating if the client should be running. */
    private volatile boolean running = false;
    
    /** TCP socket connection to the server. */
    private Socket socket;
    
    /** Input stream reader for receiving messages. */
    private BufferedReader in;
    
    /** Output stream writer for sending messages. */
    private PrintWriter out;

    /** Executor service for connection management and retries. */
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    
    /** Executor service for reading messages asynchronously. */
    private final ExecutorService readerExec = Executors.newSingleThreadExecutor();

    /**
     * Constructs a new NetworkClient with the specified connection parameters.
     * 
     * @param host Server hostname or IP address
     * @param port Server port number
     * @param clientName Unique identifier for this client
     * @param model TelemetryModel instance to update with received data
     * @param logCb Callback function for appending log messages to UI
     * @param statusCb Callback function for updating connection status in UI
     */
    public NetworkClient(String host, int port, String clientName, TelemetryModel model,
                         Consumer<String> logCb, Consumer<String> statusCb) {
        this.host = host;
        this.port = port;
        this.clientName = clientName;
        this.model = model;
        this.logCb = logCb;
        this.statusCb = statusCb;
    }

    /**
     * Starts the network client and begins connection attempts.
     * This method is non-blocking and returns immediately.
     * Connection attempts continue in the background with automatic retry.
     */
    public void start(){
        running = true;
        Platform.runLater(() -> statusCb.accept("Connecting..."));
        scheduler.execute(this::connectWithBackoff);
    }

    /**
     * Stops the network client and shuts down all executor services.
     * Closes the socket connection and performs cleanup.
     * This method should be called when the application is closing.
     */
    public void stop(){
        running = false;
        scheduler.shutdownNow();
        readerExec.shutdownNow();
        closeSocket();
    }

    /**
     * Handles connection attempts with exponential backoff retry logic.
     * Continues trying to connect until successful or until stopped.
     * Implements automatic reconnection after disconnection.
     */
    private void connectWithBackoff(){
        int attempt = 0;
        while(running){
            attempt++;
            try {
                connect();
                Platform.runLater(() -> statusCb.accept("Connected"));
                log("Connected to " + host + ":" + port);
                listenLoop();
                // if listenLoop returns, we were disconnected
                if (!running) break;
                log("Disconnected from server, will attempt reconnect");
                Platform.runLater(() -> statusCb.accept("Disconnected, reconnecting..."));
            } catch (IOException e){
                log("Connection failed: " + e.getMessage());
                Platform.runLater(() -> statusCb.accept("Connection failed, retrying..."));
            }
            // backoff
            try {
                int wait = Math.min(30, 2 + attempt*2);
                for (int i=0;i<wait && running;i++) Thread.sleep(1000);
            } catch (InterruptedException ie){ break; }
        }
    }

    /**
     * Establishes a TCP connection to the server and sends the HELLO message.
     * Configures socket settings and initializes input/output streams.
     * 
     * @throws IOException If connection fails or I/O error occurs
     */
    private void connect() throws IOException {
        closeSocket();
        socket = new Socket(host, port);
        socket.setTcpNoDelay(true);
        in = new BufferedReader(new InputStreamReader(socket.getInputStream(), "UTF-8"));
        out = new PrintWriter(new OutputStreamWriter(socket.getOutputStream(), "UTF-8"), true);
        // send HELLO <name>
        out.printf("HELLO name=%s\n", clientName);
    }

    /**
     * Main message listening loop that processes incoming data from the server.
     * Runs in a separate thread to avoid blocking the connection management thread.
     * Messages are processed on the JavaFX Application Thread for UI updates.
     * 
     * @throws IOException If I/O error occurs during message reading
     */
    private void listenLoop() throws IOException {
        // read loop executed in readerExec so connectWithBackoff can wait
        Future<?> f = readerExec.submit(() -> {
            try {
                String line;
                while (running && (line = in.readLine()) != null){
                    final String l = line;
                    Platform.runLater(() -> processLine(l));
                }
            } catch (SocketException se){
                // socket closed or reset
                log("Socket closed: " + se.getMessage());
            } catch (IOException ioe){
                log("IO error: " + ioe.getMessage());
            } finally {
                // ensure socket closed
                closeSocket();
            }
        });
        try {
            f.get(); // wait until reader finishes or throws
        } catch (InterruptedException | ExecutionException e){
            // reader aborted
            log("Reader aborted: " + e.getMessage());
        }
    }

    /**
     * Processes a single message line received from the server.
     * Handles TLM (telemetry) messages by parsing and updating the model.
     * Also processes OK, ERR, and BYE protocol messages.
     * 
     * <p>This method runs on the JavaFX Application Thread to ensure thread-safe
     * UI updates when the model properties change.
     * 
     * @param line The message line to process (null-safe)
     */
    private void processLine(String line){
        if(line == null) return;
        line = line.trim();
        if(line.isEmpty()) return;
        log("RECV: " + line);
        if(line.startsWith("TLM")){
            Map<String,String> m = MessageParser.parseTlm(line);
            try {
                int sp = Integer.parseInt(m.getOrDefault("speed","0"));
                int bt = Integer.parseInt(m.getOrDefault("battery","0"));
                int tp = Integer.parseInt(m.getOrDefault("temp","0"));
                String dir = m.getOrDefault("dir","N");
                String ts = m.getOrDefault("ts", String.valueOf(Instant.now().getEpochSecond()));
                model.update(sp, bt, tp, dir, ts);
            } catch (NumberFormatException nfe){
                log("Malformed TLM values");
            }
        } else if(line.startsWith("OK") || line.startsWith("ERR") || line.startsWith("BYE")){
            // show in log; possible future handling
        } else {
            // other messages
        }
    }

    /**
     * Logs a message with timestamp using the provided log callback.
     * Formats the message with current local time for better readability.
     * 
     * @param s The message to log
     */
    private void log(String s){
        String msg = "[" + java.time.LocalTime.now().withNano(0) + "] " + s;
        Platform.runLater(() -> logCb.accept(msg));
    }

    /**
     * Safely closes all socket-related resources.
     * Handles input stream, output stream, and socket closure with exception suppression.
     * This method is null-safe and can be called multiple times.
     */
    private void closeSocket(){
        try { if (in != null) { in.close(); in = null; } } catch(Exception e){}
        try { if (out != null) { out.close(); out = null; } } catch(Exception e){}
        try { if (socket != null && !socket.isClosed()) socket.close(); socket = null; } catch(Exception e){}
    }
}
