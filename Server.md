
## Запуск Telegram бота
Чтобы счетчик присылал вам показания, нужно запустить TCP сервер с телеграм ботом.
в файл key запишите ключ бота, полученный от BotFather
python main.py "$(< key)" --shost 127.0.0.1 --sport 4001 --host 127.0.0.1 --port 443 --admin your_nickname
shost - адрес TCP сервера
sport - порт TCP сервера, по умолчанию 4001 
host, port - адрес, порт Телеграм бота. Соединение по webhook (требует сертификаты). Если не указать, то будет long polling.
admin - ваш ник для генерации ID, пароля счетчиков на вашем сервере.