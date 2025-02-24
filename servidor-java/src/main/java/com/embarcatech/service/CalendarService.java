package com.embarcatech.service;

import com.google.api.client.util.DateTime;
import com.google.api.services.calendar.Calendar;
import com.google.api.services.calendar.model.Event;
import com.google.api.services.calendar.model.EventDateTime;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.util.Date;

@Service
public class CalendarService {

    @Autowired
    private Calendar calendarService;

    public Event createEvent(String summary, String description, LocalDateTime startTime, LocalDateTime endTime) throws IOException {
        Event event = new Event()
                .setSummary(summary)
                .setDescription(description);

        EventDateTime start = new EventDateTime()
                .setDateTime(new DateTime(Date.from(startTime.atZone(ZoneId.systemDefault()).toInstant())));
        event.setStart(start);

        EventDateTime end = new EventDateTime()
                .setDateTime(new DateTime(Date.from(endTime.atZone(ZoneId.systemDefault()).toInstant())));
        event.setEnd(end);

        return calendarService.events().insert("primary", event).execute();
    }
}
