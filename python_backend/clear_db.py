import pymysql
from db import get_conn

def clear_data():
    try:
        conn = get_conn()
        cursor = conn.cursor()
        # TRUNCATE 会清空表内所有数据，并且重置主键自增 ID
        cursor.execute("TRUNCATE TABLE sensor_data;")
        conn.commit()
        print("✅ 数据库中的旧数据（包含初始的 26.5 和 65）已全部清空！")
    except Exception as e:
        print(f"❌ 清空失败: {e}")
    finally:
        if 'conn' in locals() and conn.open:
            conn.close()

if __name__ == '__main__':
    clear_data()
