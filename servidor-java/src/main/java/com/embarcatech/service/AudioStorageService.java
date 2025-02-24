package com.embarcatech.service;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.UUID;

@Service
public class AudioStorageService {
    private final String uploadDir;

    public AudioStorageService(@Value("${audio.upload.dir}") String uploadDir) {
        this.uploadDir = uploadDir;
        createUploadDirectory();
    }

    private void createUploadDirectory() {
        File directory = new File(uploadDir);
        if (!directory.exists()) {
            directory.mkdirs();
        }
    }

    public String storeAudio(byte[] audioData) {
        String fileName = UUID.randomUUID() + ".wav";
        File file = new File(uploadDir, fileName);

        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write(audioData);
        } catch (IOException e) {
            throw new AudioStorageException("Falha ao armazenar áudio", e);
        }
        return fileName;
    }

    public static class AudioStorageException extends RuntimeException {
        public AudioStorageException(String message, Throwable cause) {
            super(message, cause);
        }
    }
}
