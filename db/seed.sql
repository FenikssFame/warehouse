SET client_encoding = 'UTF8';

-- Демонстрационные данные для варианта 3.
-- Запускать после schema.sql. Повторный запуск не дублирует товары.

DO $$
DECLARE
    v_sale_id BIGINT;
BEGIN
    IF EXISTS (SELECT 1 FROM products) THEN
        RETURN;
    END IF;

    INSERT INTO product_categories (name) VALUES
        ('Молочные продукты'),
        ('Хлеб и выпечка'),
        ('Бакалея'),
        ('Бытовая химия')
    ON CONFLICT (name) DO NOTHING;

    INSERT INTO suppliers (name, inn, contact) VALUES
        ('ООО «Молочный путь»', '7701234567', 'sales@milk-way.ru'),
        ('ООО «ОптТорг»', '7809876543', 'zakaz@opttorg.ru'),
        ('ИП Сергеев', '770500112233', '+7 (999) 123-45-67')
    ON CONFLICT (name) DO NOTHING;

    INSERT INTO products
        (sku, name, category_id, unit, sale_price_cents, tax_rate_bp,
         shelf_life_days, min_stock)
    VALUES
        ('MILK-1L', 'Молоко 3,2%, 1 л',
         (SELECT id FROM product_categories WHERE name = 'Молочные продукты'),
         'шт', 8990, 2000, 7, 10),
        ('BREAD-1', 'Хлеб пшеничный, 400 г',
         (SELECT id FROM product_categories WHERE name = 'Хлеб и выпечка'),
         'шт', 4500, 1000, 3, 5),
        ('CHEESE-1', 'Сыр «Российский», 1 кг',
         (SELECT id FROM product_categories WHERE name = 'Молочные продукты'),
         'кг', 35000, 2000, 30, 3),
        ('COFFEE-1', 'Кофе молотый, 250 г',
         (SELECT id FROM product_categories WHERE name = 'Бакалея'),
         'шт', 49900, 2000, 365, 5),
        ('SUGAR-1', 'Сахар-песок, 1 кг',
         (SELECT id FROM product_categories WHERE name = 'Бакалея'),
         'кг', 7500, 2000, 730, 10),
        ('SOAP-1', 'Мыло хозяйственное, 200 г',
         (SELECT id FROM product_categories WHERE name = 'Бытовая химия'),
         'шт', 5900, 2000, 1095, 10)
    ON CONFLICT (sku) DO NOTHING;

    INSERT INTO supplies (supplier_id, invoice_no, supplied_at, note)
    VALUES
        ((SELECT id FROM suppliers WHERE name = 'ООО «Молочный путь»'),
         'M-1001', now() - INTERVAL '2 days', 'Молочная продукция'),
        ((SELECT id FROM suppliers WHERE name = 'ООО «Молочный путь»'),
         'M-1002', now() - INTERVAL '1 day', 'Сыр и молоко'),
        ((SELECT id FROM suppliers WHERE name = 'ООО «ОптТорг»'),
         'O-2001', now() - INTERVAL '4 days', 'Бакалея'),
        ((SELECT id FROM suppliers WHERE name = 'ИП Сергеев'),
         'S-3001', now() - INTERVAL '10 days', 'Хозяйственные товары');

    -- Молоко: две партии по разной закупочной цене.
    INSERT INTO supply_items
        (supply_id, product_id, quantity, remaining_quantity,
         unit_cost_cents, expiry_date)
    VALUES
        ((SELECT id FROM supplies WHERE invoice_no = 'M-1001'),
         (SELECT id FROM products WHERE sku = 'MILK-1L'), 60, 60, 5600,
         CURRENT_DATE + 2),
        ((SELECT id FROM supplies WHERE invoice_no = 'M-1002'),
         (SELECT id FROM products WHERE sku = 'MILK-1L'), 40, 40, 5850,
         CURRENT_DATE + 12),
        ((SELECT id FROM supplies WHERE invoice_no = 'M-1002'),
         (SELECT id FROM products WHERE sku = 'CHEESE-1'), 25, 25, 25000,
         CURRENT_DATE + 20),
        ((SELECT id FROM supplies WHERE invoice_no = 'O-2001'),
         (SELECT id FROM products WHERE sku = 'BREAD-1'), 40, 40, 2400,
         CURRENT_DATE - 1),
        ((SELECT id FROM supplies WHERE invoice_no = 'O-2001'),
         (SELECT id FROM products WHERE sku = 'SUGAR-1'), 50, 50, 5200,
         CURRENT_DATE + 500),
        ((SELECT id FROM supplies WHERE invoice_no = 'O-2001'),
         (SELECT id FROM products WHERE sku = 'COFFEE-1'), 30, 30, 36000,
         CURRENT_DATE + 365),
        ((SELECT id FROM supplies WHERE invoice_no = 'S-3001'),
         (SELECT id FROM products WHERE sku = 'SOAP-1'), 70, 70, 3200,
         CURRENT_DATE + 700);

    -- Демонстрационная продажа с уже рассчитанными налогом, себестоимостью
    -- и прибылью. Ставка НДС считается включённой в цену продажи.
    INSERT INTO sales (sold_at, note)
    VALUES (now() - INTERVAL '2 hours', 'Демонстрационная продажа')
    RETURNING id INTO v_sale_id;

    INSERT INTO sale_items
        (sale_id, product_id, quantity, unit_price_cents,
         revenue_cents, tax_cents, cost_cents, profit_cents)
    VALUES
        (v_sale_id, (SELECT id FROM products WHERE sku = 'MILK-1L'),
         10, 8990, 89900, 14983, 56000, 18917),
        (v_sale_id, (SELECT id FROM products WHERE sku = 'CHEESE-1'),
         3, 35000, 105000, 17500, 75000, 12500),
        (v_sale_id, (SELECT id FROM products WHERE sku = 'COFFEE-1'),
         2, 49900, 99800, 16633, 72000, 11167),
        (v_sale_id, (SELECT id FROM products WHERE sku = 'SUGAR-1'),
         4, 7500, 30000, 5000, 20800, 4200);

    UPDATE supply_items
       SET remaining_quantity = remaining_quantity - 10
     WHERE product_id = (SELECT id FROM products WHERE sku = 'MILK-1L')
       AND expiry_date = CURRENT_DATE + 2;

    UPDATE supply_items
       SET remaining_quantity = remaining_quantity - 3
     WHERE product_id = (SELECT id FROM products WHERE sku = 'CHEESE-1');

    UPDATE supply_items
       SET remaining_quantity = remaining_quantity - 2
     WHERE product_id = (SELECT id FROM products WHERE sku = 'COFFEE-1');

    UPDATE supply_items
       SET remaining_quantity = remaining_quantity - 4
     WHERE product_id = (SELECT id FROM products WHERE sku = 'SUGAR-1');

    -- Просроченный хлеб списан по причине истечения срока годности.
    INSERT INTO write_offs
        (product_id, supply_item_id, quantity, reason, cost_cents,
         written_off_at, comment)
    SELECT
        si.product_id, si.id, 5, 'EXPIRED', 5 * si.unit_cost_cents,
        now() - INTERVAL '3 hours', 'Истёк срок годности партии'
    FROM supply_items si
    WHERE si.product_id = (SELECT id FROM products WHERE sku = 'BREAD-1')
      AND si.expiry_date = CURRENT_DATE - 1;

    UPDATE supply_items
       SET remaining_quantity = remaining_quantity - 5
     WHERE product_id = (SELECT id FROM products WHERE sku = 'BREAD-1')
       AND expiry_date = CURRENT_DATE - 1;
END $$;
