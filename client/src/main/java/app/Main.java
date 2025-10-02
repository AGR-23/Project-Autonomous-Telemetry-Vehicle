package app;

import javafx.application.Application;
import javafx.stage.Stage;
import net.NetworkClient;
import model.TelemetryModel;
import ui.ObserverUI;

/**
 * Main application class for the Autonomous Vehicle Telemetry Observer Client.
 * This JavaFX application connects to a telemetry server and displays real-time
 * vehicle data including speed, battery level, temperature, and direction.
 * 
 * <p>The application accepts command line arguments for server connection:
 * <ul>
 *   <li>Server IP address (required)</li>
 *   <li>Server port (required)</li>
 *   <li>Client name (optional, defaults to "observer")</li>
 * </ul>
 * 
 * @author Autonomous Vehicle Team
 * @version 1.0
 * @since 2025
 */
public class Main extends Application {
    /**
     * Default server IP address for telemetry connection.
     */
    private static String serverIp = "127.0.0.1";
    
    /**
     * Default server port for telemetry connection.
     */
    private static int serverPort = 9000;
    
    /**
     * Default client name identifier.
     */
    private static String clientName = "observer";

    /**
     * Network client instance for handling server communication.
     */
    private NetworkClient netClient;

    /**
     * Main entry point of the application.
     * Parses command line arguments for server connection parameters and launches the JavaFX application.
     * 
     * @param args Command line arguments:
     *             args[0] - Server IP address (required)
     *             args[1] - Server port number (required)
     *             args[2] - Client name (optional, defaults to "observer")
     */
    public static void main(String[] args){
        if (args.length >= 2) {
            serverIp = args[0];
            serverPort = Integer.parseInt(args[1]);
            if (args.length >= 3) clientName = args[2];
        } else {
            System.err.println("Usage: java -jar observer-client.jar <server_ip> <port> [name]");
            System.exit(1);
        }
        launch(args);
    }

    /**
     * Initializes and starts the JavaFX application.
     * Creates the telemetry model, user interface, and network client.
     * Establishes connection to the telemetry server and sets up cleanup on application close.
     * 
     * @param stage The primary stage for this application
     * @throws Exception If there's an error during application startup
     */
    @Override
    public void start(Stage stage) throws Exception {
        TelemetryModel model = new TelemetryModel();
        ObserverUI ui = new ObserverUI(model);

        stage.setTitle("Autonomous Vehicle - OBSERVER (" + clientName + ")");
        stage.setScene(ui.buildScene());
        stage.setWidth(700);
        stage.setHeight(450);
        stage.show();

        // Network client
        netClient = new NetworkClient(serverIp, serverPort, clientName, model, ui::appendLog, ui::setStatus);
        netClient.start();

        // on close cleanup
        stage.setOnCloseRequest(ev -> {
            netClient.stop();
        });
    }
}
