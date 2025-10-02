package ui;

import javafx.geometry.Insets;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.scene.text.Font;
import model.TelemetryModel;

import java.util.function.Consumer;

/**
 * User Interface class for the Autonomous Vehicle Telemetry Observer.
 * This class creates and manages the JavaFX interface that displays real-time
 * telemetry data and connection status.
 * 
 * <p>The UI consists of:
 * <ul>
 *   <li>Top section: Connection status and last update timestamp</li>
 *   <li>Center-left: Telemetry data display (speed, battery, temperature, direction)</li>
 *   <li>Center-right: Real-time log area for network events</li>
 *   <li>Bottom: Help text explaining the application purpose</li>
 * </ul>
 * 
 * <p>The interface uses JavaFX property binding to automatically update
 * displayed values when the telemetry model changes.
 * 
 * @author Autonomous Vehicle Team
 * @version 1.0
 * @since 2025
 */
public class ObserverUI {
    /** Reference to the telemetry data model for property binding. */
    private final TelemetryModel model;
    
    /** Text area for displaying network and application log messages. */
    private final TextArea logArea = new TextArea();
    
    /** Label for displaying current connection status. */
    private final Label statusLabel = new Label("Stopped");

    /**
     * Constructs a new ObserverUI with the specified telemetry model.
     * 
     * @param model The TelemetryModel instance to bind to UI components
     */
    public ObserverUI(TelemetryModel model){
        this.model = model;
    }

    /**
     * Builds and returns the complete JavaFX scene for the application.
     * Creates all UI components and sets up property bindings with the telemetry model.
     * 
     * <p>The scene layout includes:
     * <ul>
     *   <li>Status bar with connection status and timestamp</li>
     *   <li>Telemetry display grid with speed, battery, temperature, and direction</li>
     *   <li>Log area for real-time message display</li>
     *   <li>Help text at the bottom</li>
     * </ul>
     * 
     * @return JavaFX Scene ready to be displayed in a Stage
     */
    public Scene buildScene(){
        // Top: status and last update
        HBox top = new HBox(12);
        top.setPadding(new Insets(10));
        Label statusTitle = new Label("Status:");
        statusTitle.setFont(Font.font(14));
        statusLabel.setFont(Font.font(14));
        Label lastTs = new Label();
        lastTs.setFont(Font.font(13));
        lastTs.textProperty().bind(model.lastTsProperty());

        top.getChildren().addAll(statusTitle, statusLabel, new Label("   LastTS:"), lastTs);

        // Center: telemetry indicators
        GridPane grid = new GridPane();
        grid.setHgap(12); grid.setVgap(12); grid.setPadding(new Insets(10));

        Label speedLabel = new Label();
        speedLabel.setFont(Font.font(32));
        speedLabel.textProperty().bind(model.speedProperty().asString().concat(" km/h"));

        Label batteryLabel = new Label();
        batteryLabel.setFont(Font.font(20));
        batteryLabel.textProperty().bind(model.batteryProperty().asString().concat(" %"));

        Label tempLabel = new Label();
        tempLabel.textProperty().bind(model.tempProperty().asString().concat(" °C"));

        Label dirLabel = new Label();
        dirLabel.setFont(Font.font(20));
        dirLabel.textProperty().bind(model.dirProperty());

        grid.add(new Label("Speed"), 0, 0);
        grid.add(speedLabel, 1, 0);
        grid.add(new Label("Battery"), 0, 1);
        grid.add(batteryLabel, 1, 1);
        grid.add(new Label("Temp"), 0, 2);
        grid.add(tempLabel, 1, 2);
        grid.add(new Label("Direction"), 0, 3);
        grid.add(dirLabel, 1, 3);

        // Right: log area
        logArea.setEditable(false);
        logArea.setWrapText(true);
        logArea.setPrefWidth(320);
        logArea.setPrefHeight(260);

        VBox right = new VBox(6);
        right.setPadding(new Insets(10));
        right.getChildren().addAll(new Label("Log"), logArea);

        HBox center = new HBox(12);
        center.setPadding(new Insets(10));
        center.getChildren().addAll(grid, right);

        BorderPane root = new BorderPane();
        root.setTop(top);
        root.setCenter(center);
        // bottom help
        Label help = new Label("Observer: solo recepción de telemetría. Conecta al servidor y muestra TLM cada 10s.");
        help.setPadding(new Insets(8));
        root.setBottom(help);

        return new Scene(root);
    }

    /**
     * Appends a new log message to the log area.
     * The message is automatically followed by a newline character.
     * This method is thread-safe and can be called from any thread.
     * 
     * @param s The log message to append
     */
    public void appendLog(String s){
        logArea.appendText(s + "\n");
    }
    
    /**
     * Updates the connection status label with a new status message.
     * This method is typically called by the NetworkClient to indicate
     * connection state changes.
     * 
     * @param s The status message to display (e.g., "Connected", "Disconnected", "Connecting...")
     */
    public void setStatus(String s){
        statusLabel.setText(s);
    }
}
