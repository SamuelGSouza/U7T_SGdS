package com.embarcatech.service;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.time.temporal.ChronoUnit;
import java.util.Arrays;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

@Service
public class AudioProcessingService {

    private static final String AUDIO_UPLOADS_DIR = "audio-uploads";
    private static final Pattern DATE_PATTERN = Pattern.compile(
        "\\b(\\d{1,2})\\s+de\\s+(janeiro|fevereiro|março|abril|maio|junho|julho|agosto|setembro|outubro|novembro|dezembro)\\s+(?:de\\s+)?(\\d{4})?\\b",
        Pattern.CASE_INSENSITIVE
    );
    private static final Pattern TIME_PATTERN = Pattern.compile(
        "\\b(\\d{1,2})(?::|h|\\s*horas?)?\\s*(\\d{2})?\\b"
    );

    @Autowired
    private CalendarService calendarService;

    @Autowired
    private SpeechToTextService speechToTextService;

    @Scheduled(fixedRate = 300000) // Run every 5 minutes
    public void processNewAudioFiles() {
        File directory = new File(AUDIO_UPLOADS_DIR);
        if (!directory.exists()) {
            directory.mkdirs();
            return;
        }

        File[] files = directory.listFiles((dir, name) -> name.toLowerCase().endsWith(".wav"));
        
        if (files == null) return;

        Arrays.stream(files).forEach(this::processAudioFile);
    }

    private void processAudioFile(File audioFile) {
        try {
            String transcription = speechToTextService.transcribeAudio(audioFile.toPath());
            System.out.println("Transcrição: " + transcription);

            LocalDateTime eventDateTime = extractDateTimeFromText(transcription);
            if (eventDateTime == null) {
                System.out.println("Não foi possível extrair data e hora do áudio: " + audioFile.getName());
                return;
            }

            calendarService.createEvent(
                "Agendamento - " + audioFile.getName(),
                "Transcrição: " + transcription,
                eventDateTime,
                eventDateTime.plus(1, ChronoUnit.HOURS)
            );

            // Move to processed directory
            File processedDir = new File(AUDIO_UPLOADS_DIR + "/processed");
            if (!processedDir.exists()) {
                processedDir.mkdirs();
            }
            audioFile.renameTo(new File(processedDir, audioFile.getName()));

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private LocalDateTime extractDateTimeFromText(String text) {
        Matcher dateMatcher = DATE_PATTERN.matcher(text);
        Matcher timeMatcher = TIME_PATTERN.matcher(text);
        
        if (!dateMatcher.find() || !timeMatcher.find()) {
            return null;
        }

        try {
            int day = Integer.parseInt(dateMatcher.group(1));
            String month = convertMonth(dateMatcher.group(2));
            int year = dateMatcher.group(3) != null ? 
                      Integer.parseInt(dateMatcher.group(3)) : 
                      LocalDateTime.now().getYear();

            int hour = Integer.parseInt(timeMatcher.group(1));
            int minute = timeMatcher.group(2) != null ? 
                        Integer.parseInt(timeMatcher.group(2)) : 
                        0;

            return LocalDateTime.parse(
                String.format("%d-%s-%02d %02d:%02d", year, month, day, hour, minute),
                DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm")
            );
        } catch (DateTimeParseException | NumberFormatException e) {
            return null;
        }
    }

    private String convertMonth(String month) {
        switch (month.toLowerCase()) {
            case "janeiro": return "01";
            case "fevereiro": return "02";
            case "março": return "03";
            case "abril": return "04";
            case "maio": return "05";
            case "junho": return "06";
            case "julho": return "07";
            case "agosto": return "08";
            case "setembro": return "09";
            case "outubro": return "10";
            case "novembro": return "11";
            case "dezembro": return "12";
            default: return "01";
        }
    }
}
