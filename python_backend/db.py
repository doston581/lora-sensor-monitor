import pymysql


def get_conn():
    """
    建立并返回数据库连接对象 (Connection Object)
    """
    return pymysql.connect(
        host="localhost",
        user="root",
        password="Lhh20040824",
        database="lora_db"
    )


# ==========================================
# 独立测试区域 (仅在直接运行 db.py 时执行)
# ==========================================
if __name__ == '__main__':
    try:
        # 1. 实例化连接 (Instantiation)
        print("正在连接数据库...")
        conn = get_conn()

        # 2. 生成操作游标 (Generate Cursor)
        cursor = conn.cursor()

        # 3. 执行 SQL 语句
        print("执行插入操作...")
        sql = "INSERT INTO sensor_data (node_id, temperature, humidity) VALUES (%s, %s, %s)"
        cursor.execute(sql, ('01', 26.5, 65))

        # 4. 事务提交 (Commit Transaction / Xác nhận giao dịch) - 不提交数据不会落盘
        conn.commit()
        print("写入成功！请打开 Navicat 刷新查看数据。")

    except pymysql.err.OperationalError as e:
        print(f"[致命错误] 数据库连接失败: {e}")
        print("👉 检查：密码对不对？MySQL 服务有没有启动？lora_db 数据库建了没？")
    except Exception as e:
        print(f"[未知异常]: {e}")
    finally:
        # 5. 资源释放 (Resource Release)
        if 'cursor' in locals():
            cursor.close()
        if 'conn' in locals() and conn.open:
            conn.close()