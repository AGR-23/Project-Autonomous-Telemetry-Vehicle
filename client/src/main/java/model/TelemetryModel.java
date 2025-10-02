package model;

import javafx.beans.property.*;

/**
 * Model class that holds telemetry data for an autonomous vehicle.
 * This class uses JavaFX properties to enable data binding with UI components.
 * 
 * <p>The model tracks the following vehicle parameters:
 * <ul>
 *   <li>Speed in km/h</li>
 *   <li>Battery level as percentage</li>
 *   <li>Temperature in Celsius</li>
 *   <li>Direction (N, S, E, W, etc.)</li>
 *   <li>Last timestamp of data update</li>
 * </ul>
 * 
 * @author Autonomous Vehicle Team
 * @version 1.0
 * @since 2025
 */
public class TelemetryModel {
    /**
     * Vehicle speed property in km/h (default: 0).
     */
    private final IntegerProperty speed = new SimpleIntegerProperty(0);
    
    /**
     * Battery level property as percentage (default: 100).
     */
    private final IntegerProperty battery = new SimpleIntegerProperty(100);
    
    /**
     * Vehicle temperature property in Celsius (default: 35).
     */
    private final IntegerProperty temp = new SimpleIntegerProperty(35);
    
    /**
     * Vehicle direction property (default: "N" for North).
     */
    private final StringProperty dir = new SimpleStringProperty("N");
    
    /**
     * Last timestamp property indicating when data was last updated (default: "-").
     */
    private final StringProperty lastTs = new SimpleStringProperty("-");

    /**
     * Gets the speed property for data binding.
     * 
     * @return IntegerProperty representing vehicle speed in km/h
     */
    public IntegerProperty speedProperty(){ return speed; }
    
    /**
     * Gets the battery property for data binding.
     * 
     * @return IntegerProperty representing battery level as percentage
     */
    public IntegerProperty batteryProperty(){ return battery; }
    
    /**
     * Gets the temperature property for data binding.
     * 
     * @return IntegerProperty representing vehicle temperature in Celsius
     */
    public IntegerProperty tempProperty(){ return temp; }
    
    /**
     * Gets the direction property for data binding.
     * 
     * @return StringProperty representing vehicle direction
     */
    public StringProperty dirProperty(){ return dir; }
    
    /**
     * Gets the last timestamp property for data binding.
     * 
     * @return StringProperty representing the last update timestamp
     */
    public StringProperty lastTsProperty(){ return lastTs; }

    /**
     * Updates all telemetry values at once.
     * This method is typically called when new telemetry data is received from the server.
     * 
     * @param s Speed in km/h
     * @param b Battery level as percentage (0-100)
     * @param t Temperature in Celsius
     * @param d Direction (e.g., "N", "S", "E", "W", "NE", etc.)
     * @param ts Timestamp string representing when the data was recorded
     */
    public void update(int s, int b, int t, String d, String ts){
        speed.set(s); battery.set(b); temp.set(t); dir.set(d); lastTs.set(ts);
    }
}
