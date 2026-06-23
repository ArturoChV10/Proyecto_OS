# Ejemplos de uso

# 1. Crear un bucket
./aws-s3 mb s3://mi-bucket

# 2. Subir un archivo
./aws-s3 cp documento.pdf s3://mi-bucket/docs/manual.pdf

# 3. Listar el contenido del bucket
./aws-s3 ls s3://mi-bucket/

# 4. Descargar un archivo
./aws-s3 cp s3://mi-bucket/docs/manual.pdf ./manual_local.pdf

# 5. Eliminar un archivo
./aws-s3 rm s3://mi-bucket/docs/manual.pdf

# 6. Eliminar recursivamente una carpeta simulada
./aws-s3 rm s3://mi-bucket/docs/ --recursive

# 7. Mover un objeto dentro del mismo bucket (renombrar)
./aws-s3 mv s3://mi-bucket/antiguo.txt s3://mi-bucket/nuevo.txt

# 8. Eliminar el bucket vacío
./aws-s3 rb s3://mi-bucket

# 9. Forzar eliminación del bucket con contenido
./aws-s3 rb s3://mi-bucket --force
