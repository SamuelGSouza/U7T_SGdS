package com.embarcatech.controller;

import com.embarcatech.service.AudioStorageService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RestController;

import java.io.IOException;

@RestController
public class AudioUploadController {

    private final AudioStorageService audioStorageService;

    public AudioUploadController(AudioStorageService audioStorageService) {
        this.audioStorageService = audioStorageService;
    }

    @PostMapping(value = "/upload_audio", consumes = "audio/wav")
    public ResponseEntity<String> uploadAudio(HttpServletRequest request) {
        try {
            byte[] audioData = request.getInputStream().readAllBytes();
            String fileName = audioStorageService.storeAudio(audioData);
            return ResponseEntity.ok("Áudio armazenado: " + fileName);
        } catch (IOException e) {
            System.out.println("Erro na leitura do áudio" + e);
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body("Erro no upload");
        }
    }
}