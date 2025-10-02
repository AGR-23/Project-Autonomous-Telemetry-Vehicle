package net;

import java.util.HashMap;
import java.util.Map;

/**
 * Utility class for parsing telemetry messages received from the server.
 * This class handles the parsing of TLM (Telemetry) protocol messages.
 * 
 * <p>The expected message format is:
 * <pre>
 * TLM speed=12;battery=78;temp=35;dir=N;ts=1695123456
 * </pre>
 * 
 * <p>Each key-value pair is separated by semicolons, and each pair uses
 * the format "key=value".
 * 
 * @author Autonomous Vehicle Team
 * @version 1.0
 * @since 2025
 */
public class MessageParser {
    /**
     * Parses a TLM (Telemetry) message line and extracts key-value pairs.
     * 
     * <p>Expected format: "TLM speed=12;battery=78;temp=35;dir=N;ts=169..."
     * 
     * <p>The method:
     * <ul>
     *   <li>Validates that the line starts with "TLM"</li>
     *   <li>Splits the content by semicolons</li>
     *   <li>Extracts key-value pairs separated by "="</li>
     *   <li>Returns a map with all parsed values</li>
     * </ul>
     * 
     * @param line The telemetry message line to parse (can be null)
     * @return Map containing key-value pairs from the message.
     *         Returns empty map if line is null, empty, or doesn't start with "TLM"
     * 
     * @example
     * <pre>
     * Map&lt;String,String&gt; data = MessageParser.parseTlm("TLM speed=25;battery=85;temp=40;dir=NE;ts=1695123456");
     * // data.get("speed") returns "25"
     * // data.get("battery") returns "85"
     * </pre>
     */
    public static Map<String,String> parseTlm(String line){
        Map<String,String> out = new HashMap<>();
        if (line == null) return out;
        line = line.trim();
        if (!line.startsWith("TLM")) return out;
        String body = line.length() > 3 ? line.substring(3).trim() : "";
        // remove optional leading whitespace
        if (body.startsWith(" ")) body = body.substring(1);
        // split by ';'
        String[] parts = body.split(";");
        for(String p : parts){
            p = p.trim();
            if(p.isEmpty()) continue;
            int eq = p.indexOf('=');
            if(eq>0){
                String k = p.substring(0,eq).trim();
                String v = p.substring(eq+1).trim();
                out.put(k, v);
            }
        }
        return out;
    }
}
